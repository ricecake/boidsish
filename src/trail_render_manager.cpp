#include "trail_render_manager.h"

#include <algorithm>
#include <cstring>
#include <iostream>

#include "logger.h"
#include <shader.h>

namespace Boidsish {

	void TrailRenderManager::PersistentBuffer::Create(GLenum target, size_t size) {
		Size = size;
		glGenBuffers(1, &ID);
		glBindBuffer(target, ID);
		GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
		glBufferStorage(target, size, nullptr, flags);
		MappedPtr = glMapBufferRange(target, 0, size, flags);
	}

	void TrailRenderManager::PersistentBuffer::Destroy() {
		if (ID != 0) {
			glDeleteBuffers(1, &ID);
			ID = 0;
			MappedPtr = nullptr;
		}
	}

	TrailRenderManager::TrailRenderManager() {
		glGenVertexArrays(1, &vao_);
		glBindVertexArray(vao_);

		vertex_capacity_ = INITIAL_VERTEX_CAPACITY;
		size_t vbo_size = vertex_capacity_ * FLOATS_PER_VERTEX * sizeof(float);
		size_t params_size = max_trails_capacity_ * sizeof(TrailParams);
		size_t commands_size = max_trails_capacity_ * 2 * sizeof(DrawArraysIndirectCommand) * MAX_PASSES;

		for (int i = 0; i < NUM_FRAMES; ++i) {
			vbo_persistent_[i].Create(GL_ARRAY_BUFFER, vbo_size);
			params_ssbo_persistent_[i].Create(GL_SHADER_STORAGE_BUFFER, params_size);
			draw_command_buffer_persistent_[i].Create(GL_DRAW_INDIRECT_BUFFER, commands_size);
		}

		glBindBuffer(GL_ARRAY_BUFFER, vbo_persistent_[0].ID);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, FLOATS_PER_VERTEX * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, FLOATS_PER_VERTEX * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, FLOATS_PER_VERTEX * sizeof(float), (void*)(6 * sizeof(float)));
		glEnableVertexAttribArray(2);

		glGenBuffers(1, &trail_indices_vbo_);
		glBindBuffer(GL_ARRAY_BUFFER, trail_indices_vbo_);
		trail_indices_.resize(max_trails_capacity_);
		for (int i = 0; i < (int)max_trails_capacity_; ++i)
			trail_indices_[i] = i;
		glBufferData(GL_ARRAY_BUFFER, trail_indices_.size() * sizeof(int), trail_indices_.data(), GL_STATIC_DRAW);

		glVertexAttribIPointer(3, 1, GL_INT, 0, (void*)0);
		glEnableVertexAttribArray(3);
		glVertexAttribDivisor(3, 1);

		glBindVertexArray(0);
	}

	TrailRenderManager::~TrailRenderManager() {
		if (vao_)
			glDeleteVertexArrays(1, &vao_);
		if (trail_indices_vbo_)
			glDeleteBuffers(1, &trail_indices_vbo_);

		for (int i = 0; i < NUM_FRAMES; ++i) {
			vbo_persistent_[i].Destroy();
			params_ssbo_persistent_[i].Destroy();
			draw_command_buffer_persistent_[i].Destroy();
			if (frame_fences_[i]) {
				glDeleteSync(frame_fences_[i]);
			}
		}
	}

	bool TrailRenderManager::RegisterTrail(int trail_id, size_t max_vertices) {
		std::lock_guard<std::mutex> lock(mutex_);

		if (trail_id_to_list_index_.count(trail_id)) {
			return false;
		}

		size_t vertex_offset = vertex_usage_;
		for (auto it = free_list_.begin(); it != free_list_.end(); ++it) {
			if (it->size >= max_vertices) {
				vertex_offset = it->offset;
				if (it->size == max_vertices) {
					free_list_.erase(it);
				} else {
					it->offset += max_vertices;
					it->size -= max_vertices;
				}
				break;
			}
		}

		if (vertex_offset == vertex_usage_) {
			vertex_usage_ += max_vertices;
		}

		if (vertex_usage_ > vertex_capacity_) {
			EnsureBufferCapacity(vertex_usage_);
		}

		TrailAllocation allocation{};
		allocation.vertex_offset = vertex_offset;
		allocation.max_vertices = max_vertices;
		allocation.vertices.resize(max_vertices * FLOATS_PER_VERTEX);

		int index;
		if (!free_indices_.empty()) {
			index = free_indices_.back();
			free_indices_.pop_back();
		} else {
			index = next_index_++;
		}

		if (index >= (int)max_trails_capacity_) {
			size_t old_capacity = max_trails_capacity_;
			max_trails_capacity_ *= 2;

			size_t params_size = max_trails_capacity_ * sizeof(TrailParams);
			size_t commands_size = max_trails_capacity_ * 2 * sizeof(DrawArraysIndirectCommand) * MAX_PASSES;

			for (int i = 0; i < NUM_FRAMES; ++i) {
				PersistentBuffer new_params, new_commands;
				new_params.Create(GL_SHADER_STORAGE_BUFFER, params_size);
				new_commands.Create(GL_DRAW_INDIRECT_BUFFER, commands_size);

				std::memcpy(new_params.MappedPtr, params_ssbo_persistent_[i].MappedPtr, old_capacity * sizeof(TrailParams));
				// commands don't need copying as they are rebuilt every frame

				params_ssbo_persistent_[i].Destroy();
				draw_command_buffer_persistent_[i].Destroy();

				params_ssbo_persistent_[i] = new_params;
				draw_command_buffer_persistent_[i] = new_commands;
			}

			// Update trail indices VBO
			trail_indices_.resize(max_trails_capacity_);
			for (int i = (int)old_capacity; i < (int)max_trails_capacity_; ++i)
				trail_indices_[i] = i;

			glBindBuffer(GL_ARRAY_BUFFER, trail_indices_vbo_);
			glBufferData(GL_ARRAY_BUFFER, trail_indices_.size() * sizeof(int), trail_indices_.data(), GL_STATIC_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}

		trail_id_to_list_index_[trail_id] = active_trails_list_.size();
		active_trails_list_.push_back({trail_id, allocation, index});

		return true;
	}

	void TrailRenderManager::UnregisterTrail(int trail_id) {
		std::lock_guard<std::mutex> lock(mutex_);

		auto it = trail_id_to_list_index_.find(trail_id);
		if (it == trail_id_to_list_index_.end()) {
			return;
		}

		size_t list_idx = it->second;
		const auto& entry = active_trails_list_[list_idx];

		free_list_.push_back({entry.alloc.vertex_offset, entry.alloc.max_vertices});
		free_indices_.push_back(entry.index);

		// Remove from list and update map (swap with last)
		if (list_idx < active_trails_list_.size() - 1) {
			active_trails_list_[list_idx] = active_trails_list_.back();
			trail_id_to_list_index_[active_trails_list_[list_idx].id] = list_idx;
		}
		active_trails_list_.pop_back();
		trail_id_to_list_index_.erase(it);
	}

	bool TrailRenderManager::HasTrail(int trail_id) const {
		std::lock_guard<std::mutex> lock(mutex_);
		return trail_id_to_list_index_.count(trail_id) > 0;
	}

	void TrailRenderManager::UpdateTrailData(
		int                       trail_id,
		const std::vector<float>& vertices,
		size_t                    head,
		size_t                    tail,
		size_t                    vertex_count,
		bool                      is_full,
		const glm::vec3&          aabb_min,
		const glm::vec3&          aabb_max
	) {
		std::lock_guard<std::mutex> lock(mutex_);

		auto it = trail_id_to_list_index_.find(trail_id);
		if (it == trail_id_to_list_index_.end()) {
			return;
		}

		auto& alloc = active_trails_list_[it->second].alloc;

		size_t incoming_vertex_count = vertices.size() / FLOATS_PER_VERTEX;
		size_t copy_count = std::min(incoming_vertex_count, alloc.max_vertices);
		std::memcpy(alloc.vertices.data(), vertices.data(), copy_count * FLOATS_PER_VERTEX * sizeof(float));

		alloc.clean_mask = 0;
		alloc.head = std::min(head, alloc.max_vertices - 1);
		alloc.tail = std::min(tail, alloc.max_vertices - 1);
		alloc.vertex_count = std::min(vertex_count, alloc.max_vertices);
		alloc.is_full = is_full;
		alloc.aabb_min = aabb_min;
		alloc.aabb_max = aabb_max;
	}

	void TrailRenderManager::SetTrailParams(
		int   trail_id,
		bool  iridescent,
		bool  rocket_trail,
		bool  use_pbr,
		float roughness,
		float metallic,
		float base_thickness
	) {
		std::lock_guard<std::mutex> lock(mutex_);

		auto it = trail_id_to_list_index_.find(trail_id);
		if (it == trail_id_to_list_index_.end()) {
			return;
		}

		auto& alloc = active_trails_list_[it->second].alloc;
		alloc.iridescent = iridescent;
		alloc.rocket_trail = rocket_trail;
		alloc.use_pbr = use_pbr;
		alloc.roughness = roughness;
		alloc.metallic = metallic;
		alloc.base_thickness = base_thickness;
	}

	void TrailRenderManager::EnsureBufferCapacity(size_t required_vertices) {
		if (required_vertices <= vertex_capacity_) {
			return;
		}

		size_t new_capacity = static_cast<size_t>(vertex_capacity_ * GROWTH_FACTOR);
		while (new_capacity < required_vertices) {
			new_capacity = static_cast<size_t>(new_capacity * GROWTH_FACTOR);
		}

		size_t new_vbo_size = new_capacity * FLOATS_PER_VERTEX * sizeof(float);
		size_t old_vbo_size = vertex_capacity_ * FLOATS_PER_VERTEX * sizeof(float);

		for (int i = 0; i < NUM_FRAMES; ++i) {
			PersistentBuffer new_vbo;
			new_vbo.Create(GL_ARRAY_BUFFER, new_vbo_size);

			// Copy old data
			std::memcpy(new_vbo.MappedPtr, vbo_persistent_[i].MappedPtr, old_vbo_size);

			vbo_persistent_[i].Destroy();
			vbo_persistent_[i] = new_vbo;
		}
		vertex_capacity_ = new_capacity;

		// Reset clean masks as offsets might have changed (actually offsets didn't change, but we might want to be safe)
		for (auto& entry : active_trails_list_) {
			entry.alloc.clean_mask = 0;
		}

		// Update VAO binding (use any of the VBOs, Render will re-bind)
		glBindVertexArray(vao_);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_persistent_[0].ID);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, FLOATS_PER_VERTEX * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, FLOATS_PER_VERTEX * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, FLOATS_PER_VERTEX * sizeof(float), (void*)(6 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glBindVertexArray(0);
	}

	void TrailRenderManager::CommitUpdates() {
		std::lock_guard<std::mutex> lock(mutex_);

		current_frame_ = (current_frame_ + 1) % NUM_FRAMES;
		current_pass_ = 0;

		if (frame_fences_[current_frame_]) {
			glClientWaitSync(frame_fences_[current_frame_], GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000); // 1s timeout
			glDeleteSync(frame_fences_[current_frame_]);
			frame_fences_[current_frame_] = nullptr;
		}

		uint8_t      mask = (1 << current_frame_);
		uint8_t*     vbo_ptr = (uint8_t*)vbo_persistent_[current_frame_].MappedPtr;
		TrailParams* params_ptr = (TrailParams*)params_ssbo_persistent_[current_frame_].MappedPtr;

		for (auto& entry : active_trails_list_) {
			// Ensure vertex data is up to date in this frame's buffer
			if (!(entry.alloc.clean_mask & mask)) {
				size_t byte_offset = entry.alloc.vertex_offset * FLOATS_PER_VERTEX * sizeof(float);
				size_t byte_size = entry.alloc.vertices.size() * sizeof(float);
				std::memcpy(vbo_ptr + byte_offset, entry.alloc.vertices.data(), byte_size);
				entry.alloc.clean_mask |= mask;
			}

			// Update parameters for active trails
			TrailParams& p = params_ptr[entry.index];
			const auto&  alloc = entry.alloc;
			p.base_thickness = alloc.base_thickness;
			p.use_rocket_trail = alloc.rocket_trail ? 1 : 0;
			p.use_iridescence = alloc.iridescent ? 1 : 0;
			p.use_pbr = alloc.use_pbr ? 1 : 0;
			p.roughness = alloc.roughness;
			p.metallic = alloc.metallic;
			p.head = static_cast<float>(alloc.vertex_offset + alloc.head);
			p.size = static_cast<float>(alloc.vertex_count);
			p.verts_per_step = static_cast<float>((Constants::Class::Trails::Segments() + 1) * 2);
		}
	}

	void TrailRenderManager::Render(
		Shader&                         shader,
		const glm::mat4&                view,
		const glm::mat4&                projection,
		const std::optional<glm::vec4>& clip_plane,
		const std::optional<Frustum>&   frustum
	) {
		std::lock_guard<std::mutex> lock(mutex_);
		if (active_trails_list_.empty())
			return;

		if (current_pass_ >= MAX_PASSES) {
			logger::ERROR("TrailRenderManager: Max passes exceeded!");
			return;
		}

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(GL_FALSE);

		shader.use();
		shader.setMat4("view", view);
		shader.setMat4("projection", projection);
		shader.setMat4("model", glm::mat4(1.0f));
		if (clip_plane) shader.setVec4("clipPlane", *clip_plane);
		else shader.setVec4("clipPlane", glm::vec4(0, 0, 0, 0));
		shader.setInt("useVertexColor", 1);

		glBindVertexArray(vao_);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_persistent_[current_frame_].ID);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, FLOATS_PER_VERTEX * sizeof(float), (void*)0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, FLOATS_PER_VERTEX * sizeof(float), (void*)(3 * sizeof(float)));
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, FLOATS_PER_VERTEX * sizeof(float), (void*)(6 * sizeof(float)));

		size_t                     pass_offset = current_pass_ * max_trails_capacity_ * 2;
		DrawArraysIndirectCommand* cmd_ptr = (DrawArraysIndirectCommand*)draw_command_buffer_persistent_[current_frame_].MappedPtr + pass_offset;
		size_t                     draw_count = 0;

		for (const auto& entry : active_trails_list_) {
			const auto& alloc = entry.alloc;
			if (alloc.vertex_count == 0)
				continue;

			// Frustum culling for draw commands
			if (frustum && !frustum->IsBoxInFrustum(alloc.aabb_min, alloc.aabb_max)) {
				continue;
			}

			if (!alloc.is_full && alloc.tail > alloc.head) {
				DrawArraysIndirectCommand& cmd = cmd_ptr[draw_count++];
				cmd.count = static_cast<GLuint>(alloc.tail - alloc.head);
				cmd.instanceCount = 1;
				cmd.first = static_cast<GLuint>(alloc.vertex_offset + alloc.head);
				cmd.baseInstance = static_cast<GLuint>(entry.index);
			} else if (alloc.vertex_count > 0) {
				size_t first_count = alloc.max_vertices - alloc.head;
				if (first_count > 0) {
					DrawArraysIndirectCommand& cmd1 = cmd_ptr[draw_count++];
					cmd1.count = static_cast<GLuint>(first_count);
					cmd1.instanceCount = 1;
					cmd1.first = static_cast<GLuint>(alloc.vertex_offset + alloc.head);
					cmd1.baseInstance = static_cast<GLuint>(entry.index);
				}
				if (alloc.tail > 0) {
					DrawArraysIndirectCommand& cmd2 = cmd_ptr[draw_count++];
					cmd2.count = static_cast<GLuint>(alloc.tail);
					cmd2.instanceCount = 1;
					cmd2.first = static_cast<GLuint>(alloc.vertex_offset);
					cmd2.baseInstance = static_cast<GLuint>(entry.index);
				}
			}
		}

		if (draw_count > 0) {
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, params_ssbo_persistent_[current_frame_].ID);
			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, draw_command_buffer_persistent_[current_frame_].ID);
			glMultiDrawArraysIndirect(GL_TRIANGLE_STRIP, (void*)(pass_offset * sizeof(DrawArraysIndirectCommand)), static_cast<GLsizei>(draw_count), 0);
		}

		if (frame_fences_[current_frame_])
			glDeleteSync(frame_fences_[current_frame_]);
		frame_fences_[current_frame_] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		current_pass_++;

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		shader.setInt("useVertexColor", 0);
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
	}

	size_t TrailRenderManager::GetRegisteredTrailCount() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return active_trails_list_.size();
	}

	size_t TrailRenderManager::GetTotalVertexCount() const {
		std::lock_guard<std::mutex> lock(mutex_);
		size_t total = 0;
		for (const auto& entry : active_trails_list_) {
			total += entry.alloc.vertex_count;
		}
		return total;
	}

} // namespace Boidsish
