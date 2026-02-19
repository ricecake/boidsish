#include "trail_render_manager.h"

#include <algorithm>
#include <cstring>
#include <iostream>

#include "frustum.h"
#include "logger.h"
#include <shader.h>

namespace Boidsish {

	TrailRenderManager::TrailRenderManager() {
		// Create VAO
		glGenVertexArrays(1, &vao_);
		glBindVertexArray(vao_);

		// Create VBO with initial capacity using persistent mapping
		glGenBuffers(1, &vbo_);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_);
		GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
		glBufferStorage(
			GL_ARRAY_BUFFER,
			INITIAL_VERTEX_CAPACITY * FLOATS_PER_VERTEX * sizeof(float),
			nullptr,
			flags
		);
		persistent_vbo_ptr_ = (float*)glMapBufferRange(
			GL_ARRAY_BUFFER,
			0,
			INITIAL_VERTEX_CAPACITY * FLOATS_PER_VERTEX * sizeof(float),
			flags
		);
		vertex_capacity_ = INITIAL_VERTEX_CAPACITY;

		// Set up vertex attributes (matches TrailVertex: pos + normal + color)
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, FLOATS_PER_VERTEX * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, FLOATS_PER_VERTEX * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, FLOATS_PER_VERTEX * sizeof(float), (void*)(6 * sizeof(float)));
		glEnableVertexAttribArray(2);

		glBindVertexArray(0);

		// Create indirect draw command buffer
		glGenBuffers(1, &draw_command_buffer_);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, draw_command_buffer_);
		// Up to 2 commands per trail (ring buffer) + buffer for safety
		size_t command_buffer_size = MAX_TRAILS * 2 * sizeof(DrawArraysIndirectCommand);
		glBufferStorage(GL_DRAW_INDIRECT_BUFFER, command_buffer_size, nullptr, GL_DYNAMIC_DRAW);

		// Create SSBO for trail parameters
		glGenBuffers(1, &trail_params_ssbo_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, trail_params_ssbo_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_TRAILS * sizeof(TrailParamsGPU), nullptr, GL_DYNAMIC_DRAW);
		trail_params_gpu_.resize(MAX_TRAILS);

		// Create atomic counter buffer
		glGenBuffers(1, &atomic_counter_buffer_);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomic_counter_buffer_);
		glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);

		// Load compute shader
		compute_shader_ = std::make_unique<ComputeShader>("shaders/trail_cull.comp");
		if (!compute_shader_->isValid()) {
			logger::ERROR("Failed to load trail cull compute shader!");
		}

		// Initialize internal index mapping
		internal_to_trail_id_.resize(MAX_TRAILS, -1);
	}

	TrailRenderManager::~TrailRenderManager() {
		if (vbo_) {
			glBindBuffer(GL_ARRAY_BUFFER, vbo_);
			glUnmapBuffer(GL_ARRAY_BUFFER);
			glDeleteBuffers(1, &vbo_);
		}
		if (draw_command_buffer_) {
			glDeleteBuffers(1, &draw_command_buffer_);
		}
		if (vao_)
			glDeleteVertexArrays(1, &vao_);
		if (trail_params_ssbo_)
			glDeleteBuffers(1, &trail_params_ssbo_);
		if (atomic_counter_buffer_)
			glDeleteBuffers(1, &atomic_counter_buffer_);
	}

	bool TrailRenderManager::RegisterTrail(int trail_id, size_t max_vertices) {
		std::lock_guard<std::mutex> lock(mutex_);

		// If trail already exists, return false
		if (trail_allocations_.count(trail_id)) {
			return false;
		}

		// Find an internal index
		int internal_index = -1;
		for (size_t i = 0; i < MAX_TRAILS; ++i) {
			if (internal_to_trail_id_[i] == -1) {
				internal_index = (int)i;
				break;
			}
		}

		if (internal_index == -1) {
			logger::ERROR("Maximum trail capacity reached ({})", MAX_TRAILS);
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
		allocation.internal_index = internal_index;
		allocation.vertex_offset = vertex_offset;
		allocation.max_vertices = max_vertices;
		allocation.head = 0;
		allocation.tail = 0;
		allocation.vertex_count = 0;
		allocation.is_full = false;
		allocation.needs_upload = false;

		trail_allocations_[trail_id] = allocation;
		internal_to_trail_id_[internal_index] = trail_id;
		trail_id_to_internal_[trail_id] = internal_index;

		draw_commands_dirty_ = true;
		params_dirty_ = true;

		return true;
	}

	void TrailRenderManager::UnregisterTrail(int trail_id) {
		std::lock_guard<std::mutex> lock(mutex_);

		auto it = trail_allocations_.find(trail_id);
		if (it == trail_allocations_.end()) {
			return;
		}

		const auto& alloc = it->second;

		// Free internal index
		int internal_index = alloc.internal_index;
		internal_to_trail_id_[internal_index] = -1;
		trail_id_to_internal_.erase(trail_id);

		// Clear GPU params for this slot
		std::memset(&trail_params_gpu_[internal_index], 0, sizeof(TrailParamsGPU));

		// Add to free list
		free_list_.push_back({alloc.vertex_offset, alloc.max_vertices});

		trail_allocations_.erase(it);
		pending_vertex_data_.erase(trail_id);
		draw_commands_dirty_ = true;
		params_dirty_ = true;
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
		bool                      is_full,
		const glm::vec3&          min_bound,
		const glm::vec3&          max_bound
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
		alloc.min_bound = min_bound;
		alloc.max_bound = max_bound;
		alloc.needs_upload = true;

		draw_commands_dirty_ = true;
		params_dirty_ = true;
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
		params_dirty_ = true;
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
		GLuint    new_vbo;
		GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
		glGenBuffers(1, &new_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, new_vbo);
		glBufferStorage(GL_ARRAY_BUFFER, new_capacity * FLOATS_PER_VERTEX * sizeof(float), nullptr, flags);
		float* new_ptr =
			(float*)glMapBufferRange(GL_ARRAY_BUFFER, 0, new_capacity * FLOATS_PER_VERTEX * sizeof(float), flags);

		// Copy old data while old buffer is still mapped
		std::memcpy(new_ptr, persistent_vbo_ptr_, vertex_capacity_ * FLOATS_PER_VERTEX * sizeof(float));

		// Unmap and delete old buffer
		glBindBuffer(GL_ARRAY_BUFFER, vbo_);
		glUnmapBuffer(GL_ARRAY_BUFFER);
		glDeleteBuffers(1, &vbo_);

		vbo_ = new_vbo;
		persistent_vbo_ptr_ = new_ptr;
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

		if (params_dirty_) {
			_UpdateParamsSSBO();
		}

		if (pending_vertex_data_.empty()) {
			return;
		}

		// Use persistent mapped pointer for direct memory copies
		for (auto& [trail_id, vertices] : pending_vertex_data_) {
			auto alloc_it = trail_allocations_.find(trail_id);
			if (alloc_it == trail_allocations_.end()) {
				continue;
			}

			auto& alloc = alloc_it->second;
			if (!alloc.needs_upload) {
				continue;
			}

			size_t vertex_offset = alloc.vertex_offset * FLOATS_PER_VERTEX;
			size_t float_count = vertices.size();
			size_t max_float_count = alloc.max_vertices * FLOATS_PER_VERTEX;

			if (float_count > max_float_count) {
				float_count = max_float_count;
			}

			if (float_count > 0) {
				std::memcpy(persistent_vbo_ptr_ + vertex_offset, vertices.data(), float_count * sizeof(float));
			}

			alloc.needs_upload = false;
		}

		pending_vertex_data_.clear();
	}

	void TrailRenderManager::_UpdateParamsSSBO() {
		for (const auto& [trail_id, alloc] : trail_allocations_) {
			auto& p = trail_params_gpu_[alloc.internal_index];
			p.min_bound = glm::vec4(alloc.min_bound, alloc.base_thickness);
			p.max_bound = glm::vec4(alloc.max_bound, alloc.roughness);
			p.config1 = glm::uvec4(
				(uint32_t)alloc.vertex_offset,
				(uint32_t)alloc.max_vertices,
				(uint32_t)alloc.head,
				(uint32_t)alloc.tail
			);
			p.config2 = glm::uvec4(
				(uint32_t)alloc.vertex_count,
				(uint32_t)alloc.is_full,
				(uint32_t)alloc.iridescent,
				(uint32_t)alloc.rocket_trail
			);
			p.config3 = glm::vec4((float)alloc.use_pbr, alloc.metallic, 0.0f, 0.0f);
		}

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, trail_params_ssbo_);
		glBufferSubData(
			GL_SHADER_STORAGE_BUFFER,
			0,
			trail_params_gpu_.size() * sizeof(TrailParamsGPU),
			trail_params_gpu_.data()
		);
		params_dirty_ = false;
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

		// Ensure params are up to date
		if (params_dirty_) {
			_UpdateParamsSSBO();
		}

		// --- GPU Culling Pass ---
		if (compute_shader_ && compute_shader_->isValid()) {
			// Reset atomic counter
			GLuint zero = 0;
			glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomic_counter_buffer_);
			glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &zero);

			compute_shader_->use();

			// Set frustum planes
			Frustum frustum = Frustum::FromViewProjection(view, projection);
			for (int i = 0; i < 6; ++i) {
				compute_shader_->setVec4(
					"u_frustumPlanes[" + std::to_string(i) + "]",
					glm::vec4(frustum.planes[i].normal, frustum.planes[i].distance)
				);
			}
			compute_shader_->setInt("u_numTrails", (int)MAX_TRAILS);

			// Bind SSBOs and atomic counter
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, trail_params_ssbo_);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, draw_command_buffer_);
			glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, atomic_counter_buffer_);

			// Dispatch compute shader
			uint32_t num_groups = (static_cast<uint32_t>(MAX_TRAILS) + 63) / 64;
			glDispatchCompute(num_groups, 1, 1);

			// Ensure commands and counts are written before indirect draw
			glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
		} else {
			return;
		}

		// --- MDI Render Pass ---
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(GL_FALSE);

		shader.use();
		shader.setMat4("view", view);
		shader.setMat4("projection", projection);

		if (clip_plane) {
			shader.setVec4("clipPlane", *clip_plane);
		} else {
			// A plane that doesn't clip anything (0,0,0,1 means dot(pos,0)+1 = 1 > 0)
			shader.setVec4("clipPlane", glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
		}

		glBindVertexArray(vao_);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, draw_command_buffer_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, trail_params_ssbo_);

		// Use MultiDrawArraysIndirectCount if supported for fully GPU-driven rendering
		if (GLEW_ARB_indirect_parameters || GLEW_VERSION_4_6) {
			glBindBuffer(GL_PARAMETER_BUFFER, atomic_counter_buffer_);
			glMultiDrawArraysIndirectCount(GL_TRIANGLE_STRIP, 0, 0, static_cast<GLsizei>(MAX_TRAILS * 2), 0);
			glBindBuffer(GL_PARAMETER_BUFFER, 0);
		} else {
			// Fallback to readback for older hardware
			GLuint count = 0;
			glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomic_counter_buffer_);
			glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &count);

			if (count > 0) {
				glMultiDrawArraysIndirect(GL_TRIANGLE_STRIP, 0, count, 0);
			}
		}

		glBindVertexArray(0);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);

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
