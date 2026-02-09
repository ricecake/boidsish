#include "trail_render_manager.h"

#include <algorithm>
#include <iostream>

#include "logger.h"
#include <shader.h>

namespace Boidsish {

	TrailRenderManager::TrailRenderManager() {
		// Create VAO
		glGenVertexArrays(1, &vao_);
		glBindVertexArray(vao_);

		// Create VBO with initial capacity
		glGenBuffers(1, &vbo_);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_);
		glBufferData(
			GL_ARRAY_BUFFER,
			INITIAL_VERTEX_CAPACITY * FLOATS_PER_VERTEX * sizeof(float),
			nullptr,
			GL_DYNAMIC_DRAW
		);
		vertex_capacity_ = INITIAL_VERTEX_CAPACITY;

		// Set up vertex attributes (matches TrailVertex: pos + normal + color)
		// Position attribute (location 0)
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, FLOATS_PER_VERTEX * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		// Normal attribute (location 1)
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, FLOATS_PER_VERTEX * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
		// Color attribute (location 2)
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, FLOATS_PER_VERTEX * sizeof(float), (void*)(6 * sizeof(float)));
		glEnableVertexAttribArray(2);

		// DrawID attribute (location 3)
		glGenBuffers(1, &trail_indices_vbo_);
		glBindBuffer(GL_ARRAY_BUFFER, trail_indices_vbo_);
		// Initialize with some default capacity
		trail_indices_.resize(1024);
		for (int i = 0; i < 1024; ++i)
			trail_indices_[i] = i;
		glBufferData(GL_ARRAY_BUFFER, trail_indices_.size() * sizeof(int), trail_indices_.data(), GL_STATIC_DRAW);

		glVertexAttribIPointer(3, 1, GL_INT, 0, (void*)0);
		glEnableVertexAttribArray(3);
		glVertexAttribDivisor(3, 1);

		glBindVertexArray(0);

		// Create indirect draw command buffer
		glGenBuffers(1, &draw_command_buffer_);

		// Create SSBO for trail parameters
		glGenBuffers(1, &params_ssbo_);
	}

	TrailRenderManager::~TrailRenderManager() {
		if (vao_)
			glDeleteVertexArrays(1, &vao_);
		if (vbo_)
			glDeleteBuffers(1, &vbo_);
		if (draw_command_buffer_)
			glDeleteBuffers(1, &draw_command_buffer_);
		if (params_ssbo_)
			glDeleteBuffers(1, &params_ssbo_);
		if (trail_indices_vbo_)
			glDeleteBuffers(1, &trail_indices_vbo_);
	}

	bool TrailRenderManager::RegisterTrail(int trail_id, size_t max_vertices) {
		std::lock_guard<std::mutex> lock(mutex_);

		// If trail already exists, return false
		if (trail_allocations_.count(trail_id)) {
			return false;
		}

		// Try to find space in free list
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

		// If no free block found, allocate at end
		if (vertex_offset == vertex_usage_) {
			vertex_usage_ += max_vertices;
		}

		// Ensure buffer capacity
		if (vertex_usage_ > vertex_capacity_) {
			EnsureBufferCapacity(vertex_usage_);
		}

		// Store allocation info
		TrailAllocation allocation{};
		allocation.vertex_offset = vertex_offset;
		allocation.max_vertices = max_vertices;
		allocation.head = 0;
		allocation.tail = 0;
		allocation.vertex_count = 0;
		allocation.is_full = false;
		allocation.needs_upload = false;

		trail_allocations_[trail_id] = allocation;

		// Assign a dense index for the SSBO
		int index;
		if (!free_indices_.empty()) {
			index = free_indices_.back();
			free_indices_.pop_back();
		} else {
			index = next_index_++;
		}
		trail_id_to_index_[trail_id] = index;

		// Ensure trail_indices_ is large enough for the DrawID attribute
		if (index >= static_cast<int>(trail_indices_.size())) {
			size_t old_size = trail_indices_.size();
			size_t new_size = old_size * 2;
			while (index >= static_cast<int>(new_size))
				new_size *= 2;
			trail_indices_.resize(new_size);
			for (size_t i = old_size; i < new_size; ++i) {
				trail_indices_[i] = static_cast<int>(i);
			}

			glBindBuffer(GL_ARRAY_BUFFER, trail_indices_vbo_);
			glBufferData(GL_ARRAY_BUFFER, trail_indices_.size() * sizeof(int), trail_indices_.data(), GL_STATIC_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}

		draw_commands_dirty_ = true;

		return true;
	}

	void TrailRenderManager::UnregisterTrail(int trail_id) {
		std::lock_guard<std::mutex> lock(mutex_);

		auto it = trail_allocations_.find(trail_id);
		if (it == trail_allocations_.end()) {
			return;
		}

		const auto& alloc = it->second;

		// Add to free list
		free_list_.push_back({alloc.vertex_offset, alloc.max_vertices});

		trail_allocations_.erase(it);
		pending_vertex_data_.erase(trail_id);

		auto idx_it = trail_id_to_index_.find(trail_id);
		if (idx_it != trail_id_to_index_.end()) {
			free_indices_.push_back(idx_it->second);
			trail_id_to_index_.erase(idx_it);
		}

		draw_commands_dirty_ = true;
	}

	bool TrailRenderManager::HasTrail(int trail_id) const {
		std::lock_guard<std::mutex> lock(mutex_);
		return trail_allocations_.count(trail_id) > 0;
	}

	void TrailRenderManager::UpdateTrailData(
		int                       trail_id,
		const std::vector<float>& vertices,
		size_t                    head,
		size_t                    tail,
		size_t                    vertex_count,
		bool                      is_full
	) {
		std::lock_guard<std::mutex> lock(mutex_);

		auto it = trail_allocations_.find(trail_id);
		if (it == trail_allocations_.end()) {
			return;
		}

		auto& alloc = it->second;

		// Bounds check: Ensure vertex data doesn't exceed allocated capacity
		size_t incoming_vertex_count = vertices.size() / FLOATS_PER_VERTEX;
		if (incoming_vertex_count > alloc.max_vertices) {
			logger::ERROR(
				"Trail {} update exceeded allocated capacity ({} > {}) - truncating",
				trail_id,
				incoming_vertex_count,
				alloc.max_vertices
			);
			std::vector<float> truncated_vertices = vertices;
			truncated_vertices.resize(alloc.max_vertices * FLOATS_PER_VERTEX);
			pending_vertex_data_[trail_id] = std::move(truncated_vertices);
		} else {
			pending_vertex_data_[trail_id] = vertices;
		}

		alloc.head = std::min(head, alloc.max_vertices - 1);
		alloc.tail = std::min(tail, alloc.max_vertices - 1);
		alloc.vertex_count = std::min(vertex_count, alloc.max_vertices);
		alloc.is_full = is_full;
		alloc.needs_upload = true;

		draw_commands_dirty_ = true;
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

		auto it = trail_allocations_.find(trail_id);
		if (it == trail_allocations_.end()) {
			return;
		}

		auto& alloc = it->second;
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

		// Create new buffer
		GLuint new_vbo;
		glGenBuffers(1, &new_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, new_vbo);
		glBufferData(GL_ARRAY_BUFFER, new_capacity * FLOATS_PER_VERTEX * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

		// Copy old data
		glBindBuffer(GL_COPY_READ_BUFFER, vbo_);
		glCopyBufferSubData(
			GL_COPY_READ_BUFFER,
			GL_ARRAY_BUFFER,
			0,
			0,
			vertex_capacity_ * FLOATS_PER_VERTEX * sizeof(float)
		);

		glDeleteBuffers(1, &vbo_);
		vbo_ = new_vbo;
		vertex_capacity_ = new_capacity;

		// Update VAO binding
		glBindVertexArray(vao_);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_);

		// Re-setup vertex attributes
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, FLOATS_PER_VERTEX * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, FLOATS_PER_VERTEX * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, FLOATS_PER_VERTEX * sizeof(float), (void*)(6 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0); // Prevent buffer state leakage after resize
	}

	void TrailRenderManager::CommitUpdates() {
		std::lock_guard<std::mutex> lock(mutex_);

		if (pending_vertex_data_.empty()) {
			return;
		}

		glBindBuffer(GL_ARRAY_BUFFER, vbo_);

		// Upload pending vertex data
		for (auto& [trail_id, vertices] : pending_vertex_data_) {
			auto alloc_it = trail_allocations_.find(trail_id);
			if (alloc_it == trail_allocations_.end()) {
				continue;
			}

			auto& alloc = alloc_it->second;
			if (!alloc.needs_upload) {
				continue;
			}

			// Upload the entire trail data at its allocated offset
			// The vertices are already in the correct format (pos + normal + color)
			size_t byte_offset = alloc.vertex_offset * FLOATS_PER_VERTEX * sizeof(float);
			size_t byte_size = vertices.size() * sizeof(float);
			size_t max_byte_size = alloc.max_vertices * FLOATS_PER_VERTEX * sizeof(float);

			if (byte_size > max_byte_size) {
				logger::ERROR("Trail {} upload size mismatch during commit - truncating", trail_id);
				byte_size = max_byte_size;
			}

			if (byte_size > 0) {
				glBufferSubData(GL_ARRAY_BUFFER, byte_offset, byte_size, vertices.data());
			}

			alloc.needs_upload = false;
		}

		pending_vertex_data_.clear();
		glBindBuffer(GL_ARRAY_BUFFER, 0); // Prevent buffer state leakage
	}

	void TrailRenderManager::Render(
		Shader&                         shader,
		const glm::mat4&                view,
		const glm::mat4&                projection,
		const std::optional<glm::vec4>& clip_plane
	) {
		std::lock_guard<std::mutex> lock(mutex_);

		if (trail_allocations_.empty()) {
			return;
		}

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(GL_FALSE);

		shader.use();
		shader.setMat4("view", view);
		shader.setMat4("projection", projection);
		shader.setMat4("model", glm::mat4(1.0f)); // Identity - positions are world space

		if (clip_plane) {
			shader.setVec4("clipPlane", *clip_plane);
		} else {
			shader.setVec4("clipPlane", glm::vec4(0, 0, 0, 0));
		}

		shader.setInt("useVertexColor", 1);

		glBindVertexArray(vao_);

		// Build draw commands if dirty
		// Prepare trail parameters SSBO
		params_buffer_.assign(next_index_, TrailParams{});

		for (const auto& [trail_id, alloc] : trail_allocations_) {
			int index = trail_id_to_index_[trail_id];
			TrailParams& p = params_buffer_[index];
			p.base_thickness = alloc.base_thickness;
			p.use_rocket_trail = alloc.rocket_trail ? 1 : 0;
			p.use_iridescence = alloc.iridescent ? 1 : 0;
			p.use_pbr = alloc.use_pbr ? 1 : 0;
			p.roughness = alloc.roughness;
			p.metallic = alloc.metallic;
			p.head = static_cast<float>(alloc.vertex_offset + alloc.head);
			p.size = static_cast<float>(alloc.vertex_count);
		}

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, params_ssbo_);
		glBufferData(
			GL_SHADER_STORAGE_BUFFER,
			params_buffer_.size() * sizeof(TrailParams),
			params_buffer_.data(),
			GL_DYNAMIC_DRAW
		);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, params_ssbo_);

		// Build draw commands if dirty
		if (draw_commands_dirty_) {
			draw_commands_.clear();
			draw_commands_.reserve(trail_allocations_.size() * 2); // Up to 2 draws per trail for ring buffer wrap

			for (const auto& [trail_id, alloc] : trail_allocations_) {
				if (alloc.vertex_count == 0) {
					continue;
				}

				int trail_index = trail_id_to_index_[trail_id];

				// Handle ring buffer - may need 1 or 2 draw commands per trail
				if (!alloc.is_full && alloc.tail > alloc.head) {
					// Single contiguous segment
					DrawArraysIndirectCommand cmd{};
					cmd.count = static_cast<GLuint>(alloc.tail - alloc.head);
					cmd.instanceCount = 1;
					cmd.first = static_cast<GLuint>(alloc.vertex_offset + alloc.head);
					cmd.baseInstance = static_cast<GLuint>(trail_index); // Use baseInstance to identify trail
					draw_commands_.push_back(cmd);
				} else if (alloc.vertex_count > 0) {
					// Wrapped ring buffer - two segments
					// First segment: from head to end of buffer
					size_t first_count = alloc.max_vertices - alloc.head;
					if (first_count > 0) {
						DrawArraysIndirectCommand cmd1{};
						cmd1.count = static_cast<GLuint>(first_count);
						cmd1.instanceCount = 1;
						cmd1.first = static_cast<GLuint>(alloc.vertex_offset + alloc.head);
						cmd1.baseInstance = static_cast<GLuint>(trail_index);
						draw_commands_.push_back(cmd1);
					}

					// Second segment: from start of buffer to tail
					if (alloc.tail > 0) {
						DrawArraysIndirectCommand cmd2{};
						cmd2.count = static_cast<GLuint>(alloc.tail);
						cmd2.instanceCount = 1;
						cmd2.first = static_cast<GLuint>(alloc.vertex_offset);
						cmd2.baseInstance = static_cast<GLuint>(trail_index);
						draw_commands_.push_back(cmd2);
					}
				}
			}

			// Upload draw commands to GPU buffer
			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, draw_command_buffer_);
			glBufferData(
				GL_DRAW_INDIRECT_BUFFER,
				draw_commands_.size() * sizeof(DrawArraysIndirectCommand),
				draw_commands_.data(),
				GL_DYNAMIC_DRAW
			);

			draw_commands_dirty_ = false;
		}

		if (!draw_commands_.empty()) {
			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, draw_command_buffer_);
			glMultiDrawArraysIndirect(GL_TRIANGLE_STRIP, 0, static_cast<GLsizei>(draw_commands_.size()), 0);
		}

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0); // Prevent buffer state leakage
		shader.setInt("useVertexColor", 0);

		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
	}

	size_t TrailRenderManager::GetRegisteredTrailCount() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return trail_allocations_.size();
	}

	size_t TrailRenderManager::GetTotalVertexCount() const {
		std::lock_guard<std::mutex> lock(mutex_);
		size_t                      total = 0;
		for (const auto& [id, alloc] : trail_allocations_) {
			total += alloc.vertex_count;
		}
		return total;
	}

} // namespace Boidsish
