#include "trail_render_manager.h"

#include <algorithm>
#include <iostream>
#include <cstring>

#include "profiler.h"
#include "opengl_helpers.h"
#include <shader.h>

namespace Boidsish {

	TrailRenderManager::TrailRenderManager() {
		// Create VAO
		glGenVertexArrays(1, &vao_);
		glBindVertexArray(vao_);

		vbo_ring_ = std::make_unique<PersistentRingBuffer<TrailVertex>>(GL_ARRAY_BUFFER, INITIAL_VERTEX_CAPACITY);
		vertex_capacity_ = INITIAL_VERTEX_CAPACITY;

		// Set up vertex attributes (matches TrailVertex: pos + normal + color)
		// Position attribute (location 0)
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TrailVertex), (void*)0);
		glEnableVertexAttribArray(0);
		// Normal attribute (location 1)
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(TrailVertex), (void*)(offsetof(TrailVertex, normal)));
		glEnableVertexAttribArray(1);
		// Color attribute (location 2)
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(TrailVertex), (void*)(offsetof(TrailVertex, color)));
		glEnableVertexAttribArray(2);

		glBindVertexArray(0);
	}

	TrailRenderManager::~TrailRenderManager() {
		if (vao_)
			glDeleteVertexArrays(1, &vao_);
		vbo_ring_.reset();
		draw_command_ring_.reset();
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
		alloc.head = head;
		alloc.tail = tail;
		alloc.vertex_count = vertex_count;
		alloc.is_full = is_full;
		alloc.needs_upload = true;

		pending_vertex_data_[trail_id] = vertices;
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

		vbo_ring_->EnsureCapacity(new_capacity);
		vertex_capacity_ = new_capacity;

		// Update VAO binding
		glBindVertexArray(vao_);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_ring_->GetVBO());

		// Re-setup vertex attributes
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TrailVertex), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(TrailVertex), (void*)(offsetof(TrailVertex, normal)));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(TrailVertex), (void*)(offsetof(TrailVertex, color)));
		glEnableVertexAttribArray(2);
		glBindVertexArray(0);
	}

	void TrailRenderManager::CommitUpdates() {
		std::lock_guard<std::mutex> lock(mutex_);

		if (pending_vertex_data_.empty() || !vbo_ring_) {
			return;
		}

		TrailVertex* vbo_ptr = vbo_ring_->GetCurrentPtr();
		if (!vbo_ptr) return;

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
			if (!vertices.empty()) {
				memcpy(vbo_ptr + alloc.vertex_offset, vertices.data(), vertices.size() * sizeof(float));
			}

			alloc.needs_upload = false;
		}

		pending_vertex_data_.clear();
	}

	void TrailRenderManager::Render(
		Shader&                         shader,
		const glm::mat4&                view,
		const glm::mat4&                projection,
		const std::optional<glm::vec4>& clip_plane
	) {
		PROJECT_PROFILE_SCOPE("TrailRenderManager::Render");
		std::lock_guard<std::mutex> lock(mutex_);

		if (trail_allocations_.empty() || !vbo_ring_) {
			return;
		}

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
		if (draw_commands_dirty_) {
			draw_commands_.clear();
			draw_commands_.reserve(trail_allocations_.size() * 2); // Up to 2 draws per trail for ring buffer wrap

			for (const auto& [trail_id, alloc] : trail_allocations_) {
				if (alloc.vertex_count == 0) {
					continue;
				}

				// Handle ring buffer - may need 1 or 2 draw commands per trail
				if (!alloc.is_full && alloc.tail > alloc.head) {
					// Single contiguous segment
					DrawArraysIndirectCommand cmd{};
					cmd.count = static_cast<GLuint>(alloc.tail - alloc.head);
					cmd.instanceCount = 1;
					cmd.first = static_cast<GLuint>(alloc.vertex_offset + alloc.head);
					cmd.baseInstance = trail_id; // Use baseInstance to identify trail
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
						cmd1.baseInstance = trail_id;
						draw_commands_.push_back(cmd1);
					}

					// Second segment: from start of buffer to tail
					if (alloc.tail > 0) {
						DrawArraysIndirectCommand cmd2{};
						cmd2.count = static_cast<GLuint>(alloc.tail);
						cmd2.instanceCount = 1;
						cmd2.first = static_cast<GLuint>(alloc.vertex_offset);
						cmd2.baseInstance = trail_id;
						draw_commands_.push_back(cmd2);
					}
				}
			}

			// Upload draw commands to GPU buffer
			if (!draw_command_ring_) {
				draw_command_ring_ = std::make_unique<PersistentRingBuffer<DrawArraysIndirectCommand>>(GL_DRAW_INDIRECT_BUFFER, draw_commands_.size());
			}
			draw_command_ring_->EnsureCapacity(draw_commands_.size());
			DrawArraysIndirectCommand* cmd_ptr = draw_command_ring_->GetCurrentPtr();
			if (cmd_ptr) {
				memcpy(cmd_ptr, draw_commands_.data(), draw_commands_.size() * sizeof(DrawArraysIndirectCommand));
			}

			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, draw_command_ring_->GetVBO());

			draw_commands_dirty_ = false;
		}

		size_t base_offset = vbo_ring_->GetOffset() / sizeof(TrailVertex);

		for (const auto& [trail_id, alloc] : trail_allocations_) {
			if (alloc.vertex_count == 0) {
				continue;
			}

			// Set per-trail uniforms
			shader.setFloat("base_thickness", alloc.base_thickness);
			shader.setBool("useIridescence", alloc.iridescent);
			shader.setBool("useRocketTrail", alloc.rocket_trail);
			shader.setBool("usePBR", alloc.use_pbr);
			shader.setFloat("trailRoughness", alloc.roughness);
			shader.setFloat("trailMetallic", alloc.metallic);

			// trailHead must include vertex_offset AND the ring buffer offset because gl_VertexID sees the global index
			shader.setFloat("trailHead", static_cast<float>(base_offset + alloc.vertex_offset + alloc.head));
			shader.setFloat("trailSize", static_cast<float>(alloc.vertex_count));

			// Handle ring buffer rendering
			if (!alloc.is_full && alloc.tail > alloc.head) {
				// Single contiguous segment
				glDrawArrays(
					GL_TRIANGLE_STRIP,
					static_cast<GLint>(base_offset + alloc.vertex_offset + alloc.head),
					static_cast<GLsizei>(alloc.tail - alloc.head)
				);
			} else if (alloc.vertex_count > 0) {
				// Wrapped ring buffer - two segments
				size_t first_count = alloc.max_vertices - alloc.head;
				if (first_count > 0) {
					glDrawArrays(
						GL_TRIANGLE_STRIP,
						static_cast<GLint>(base_offset + alloc.vertex_offset + alloc.head),
						static_cast<GLsizei>(first_count)
					);
				}
				if (alloc.tail > 0) {
					glDrawArrays(
						GL_TRIANGLE_STRIP,
						static_cast<GLint>(base_offset + alloc.vertex_offset),
						static_cast<GLsizei>(alloc.tail)
					);
				}
			}
		}

		glBindVertexArray(0);
		shader.setInt("useVertexColor", 0);

		vbo_ring_->AdvanceFrame();
		if (draw_command_ring_) draw_command_ring_->AdvanceFrame();
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
