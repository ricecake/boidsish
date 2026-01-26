#include "terrain_render_manager.h"

#include <algorithm>
#include <iostream>
#include <optional>

#include "terrain.h"
#include <shader.h>

namespace Boidsish {

	TerrainRenderManager::TerrainRenderManager() {
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

		// Create EBO with initial capacity
		glGenBuffers(1, &ebo_);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
		glBufferData(
			GL_ELEMENT_ARRAY_BUFFER,
			INITIAL_INDEX_CAPACITY * sizeof(unsigned int),
			nullptr,
			GL_DYNAMIC_DRAW
		);
		index_capacity_ = INITIAL_INDEX_CAPACITY;

		// Set up vertex attributes
		// Position attribute (location 0)
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, FLOATS_PER_VERTEX * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		// Normal attribute (location 1)
		glVertexAttribPointer(
			1, 3, GL_FLOAT, GL_FALSE, FLOATS_PER_VERTEX * sizeof(float), (void*)(3 * sizeof(float))
		);
		glEnableVertexAttribArray(1);
		// Texture coordinate attribute (location 2)
		glVertexAttribPointer(
			2, 2, GL_FLOAT, GL_FALSE, FLOATS_PER_VERTEX * sizeof(float), (void*)(6 * sizeof(float))
		);
		glEnableVertexAttribArray(2);

		glBindVertexArray(0);

		// Create indirect draw command buffer
		glGenBuffers(1, &draw_command_buffer_);
	}

	TerrainRenderManager::~TerrainRenderManager() {
		if (vao_) glDeleteVertexArrays(1, &vao_);
		if (vbo_) glDeleteBuffers(1, &vbo_);
		if (ebo_) glDeleteBuffers(1, &ebo_);
		if (draw_command_buffer_) glDeleteBuffers(1, &draw_command_buffer_);
	}

	void TerrainRenderManager::RegisterChunk(
		std::pair<int, int>               chunk_key,
		const std::vector<float>&         vertices,
		const std::vector<unsigned int>&  indices,
		const glm::vec3&                  world_offset
	) {
		std::lock_guard<std::mutex> lock(mutex_);

		// If chunk already exists, unregister it first
		if (chunk_allocations_.count(chunk_key)) {
			// Store data for re-registration after unregister
			pending_chunk_data_[chunk_key] = {vertices, indices, world_offset};
			return;
		}

		// Calculate vertex and index counts
		size_t vertex_count = vertices.size() / FLOATS_PER_VERTEX;
		size_t index_count = indices.size();

		// Try to find space in free lists
		size_t vertex_offset = vertex_usage_;
		size_t index_offset = index_usage_;

		// Simple first-fit allocation from free list for vertices
		for (auto it = vertex_free_list_.begin(); it != vertex_free_list_.end(); ++it) {
			if (it->size >= vertex_count) {
				vertex_offset = it->offset;
				if (it->size == vertex_count) {
					vertex_free_list_.erase(it);
				} else {
					it->offset += vertex_count;
					it->size -= vertex_count;
				}
				break;
			}
		}

		// If no free block found, allocate at end
		if (vertex_offset == vertex_usage_) {
			vertex_usage_ += vertex_count;
		}

		// Same for indices
		for (auto it = index_free_list_.begin(); it != index_free_list_.end(); ++it) {
			if (it->size >= index_count) {
				index_offset = it->offset;
				if (it->size == index_count) {
					index_free_list_.erase(it);
				} else {
					it->offset += index_count;
					it->size -= index_count;
				}
				break;
			}
		}

		if (index_offset == index_usage_) {
			index_usage_ += index_count;
		}

		// Ensure buffer capacity
		if (vertex_usage_ > vertex_capacity_ || index_usage_ > index_capacity_) {
			needs_rebuild_ = true;
		}

		// Store allocation info
		ChunkAllocation allocation{};
		allocation.vertex_offset = vertex_offset;
		allocation.vertex_count = vertex_count;
		allocation.index_offset = index_offset;
		allocation.index_count = index_count;
		allocation.world_offset = world_offset;

		chunk_allocations_[chunk_key] = allocation;

		// Store pending data for upload
		pending_chunk_data_[chunk_key] = {vertices, indices, world_offset};
		pending_registrations_.push_back(chunk_key);

		draw_commands_dirty_ = true;
	}

	void TerrainRenderManager::UnregisterChunk(std::pair<int, int> chunk_key) {
		std::lock_guard<std::mutex> lock(mutex_);

		auto it = chunk_allocations_.find(chunk_key);
		if (it == chunk_allocations_.end()) {
			return;
		}

		const auto& alloc = it->second;

		// Add to free lists (could merge adjacent blocks for better memory reuse)
		vertex_free_list_.push_back({alloc.vertex_offset, alloc.vertex_count});
		index_free_list_.push_back({alloc.index_offset, alloc.index_count});

		chunk_allocations_.erase(it);
		pending_chunk_data_.erase(chunk_key);

		draw_commands_dirty_ = true;
	}

	bool TerrainRenderManager::HasChunk(std::pair<int, int> chunk_key) const {
		std::lock_guard<std::mutex> lock(mutex_);
		return chunk_allocations_.count(chunk_key) > 0;
	}

	void TerrainRenderManager::EnsureBufferCapacity(size_t required_vertices, size_t required_indices) {
		bool need_vertex_resize = required_vertices > vertex_capacity_;
		bool need_index_resize = required_indices > index_capacity_;

		if (!need_vertex_resize && !need_index_resize) {
			return;
		}

		if (need_vertex_resize) {
			size_t new_capacity = static_cast<size_t>(vertex_capacity_ * GROWTH_FACTOR);
			while (new_capacity < required_vertices) {
				new_capacity = static_cast<size_t>(new_capacity * GROWTH_FACTOR);
			}

			// Create new buffer
			GLuint new_vbo;
			glGenBuffers(1, &new_vbo);
			glBindBuffer(GL_ARRAY_BUFFER, new_vbo);
			glBufferData(
				GL_ARRAY_BUFFER,
				new_capacity * FLOATS_PER_VERTEX * sizeof(float),
				nullptr,
				GL_DYNAMIC_DRAW
			);

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
			glVertexAttribPointer(
				1, 3, GL_FLOAT, GL_FALSE, FLOATS_PER_VERTEX * sizeof(float), (void*)(3 * sizeof(float))
			);
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(
				2, 2, GL_FLOAT, GL_FALSE, FLOATS_PER_VERTEX * sizeof(float), (void*)(6 * sizeof(float))
			);
			glEnableVertexAttribArray(2);
			glBindVertexArray(0);
		}

		if (need_index_resize) {
			size_t new_capacity = static_cast<size_t>(index_capacity_ * GROWTH_FACTOR);
			while (new_capacity < required_indices) {
				new_capacity = static_cast<size_t>(new_capacity * GROWTH_FACTOR);
			}

			GLuint new_ebo;
			glGenBuffers(1, &new_ebo);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, new_ebo);
			glBufferData(
				GL_ELEMENT_ARRAY_BUFFER,
				new_capacity * sizeof(unsigned int),
				nullptr,
				GL_DYNAMIC_DRAW
			);

			// Copy old data
			glBindBuffer(GL_COPY_READ_BUFFER, ebo_);
			glCopyBufferSubData(
				GL_COPY_READ_BUFFER,
				GL_ELEMENT_ARRAY_BUFFER,
				0,
				0,
				index_capacity_ * sizeof(unsigned int)
			);

			glDeleteBuffers(1, &ebo_);
			ebo_ = new_ebo;
			index_capacity_ = new_capacity;

			// Update VAO EBO binding
			glBindVertexArray(vao_);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
			glBindVertexArray(0);
		}
	}

	void TerrainRenderManager::UploadChunkData(
		const ChunkAllocation&           allocation,
		const std::vector<float>&        vertices,
		const std::vector<unsigned int>& indices
	) {
		// Pre-transform vertices to world space so we don't need per-chunk model matrix
		// This enables true single draw call via glMultiDrawElementsIndirect
		std::vector<float> world_vertices = vertices;
		size_t vertex_count = vertices.size() / FLOATS_PER_VERTEX;
		for (size_t i = 0; i < vertex_count; ++i) {
			size_t base = i * FLOATS_PER_VERTEX;
			// Transform position (first 3 floats)
			world_vertices[base + 0] += allocation.world_offset.x;
			world_vertices[base + 1] += allocation.world_offset.y;
			world_vertices[base + 2] += allocation.world_offset.z;
			// Normals (next 3 floats) don't need transformation for translation
			// Texcoords (last 2 floats) stay the same
		}

		// Upload vertex data (now in world space)
		glBindBuffer(GL_ARRAY_BUFFER, vbo_);
		glBufferSubData(
			GL_ARRAY_BUFFER,
			allocation.vertex_offset * FLOATS_PER_VERTEX * sizeof(float),
			world_vertices.size() * sizeof(float),
			world_vertices.data()
		);

		// Upload index data
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
		glBufferSubData(
			GL_ELEMENT_ARRAY_BUFFER,
			allocation.index_offset * sizeof(unsigned int),
			indices.size() * sizeof(unsigned int),
			indices.data()
		);
	}

	void TerrainRenderManager::CommitUpdates() {
		std::lock_guard<std::mutex> lock(mutex_);

		if (pending_registrations_.empty() && !needs_rebuild_) {
			return;
		}

		// Ensure buffer capacity
		EnsureBufferCapacity(vertex_usage_, index_usage_);

		// Upload pending chunk data
		for (const auto& chunk_key : pending_registrations_) {
			auto data_it = pending_chunk_data_.find(chunk_key);
			auto alloc_it = chunk_allocations_.find(chunk_key);
			if (data_it != pending_chunk_data_.end() && alloc_it != chunk_allocations_.end()) {
				const auto& [vertices, indices, world_offset] = data_it->second;
				UploadChunkData(alloc_it->second, vertices, indices);
			}
		}

		pending_registrations_.clear();
		pending_chunk_data_.clear();
		needs_rebuild_ = false;
	}

	void TerrainRenderManager::Render(
		Shader&                         shader,
		const glm::mat4&                view,
		const glm::mat4&                projection,
		const std::optional<glm::vec4>& clip_plane,
		float                           tess_quality_multiplier
	) {
		std::lock_guard<std::mutex> lock(mutex_);

		if (chunk_allocations_.empty()) {
			return;
		}

		shader.use();
		// Identity model matrix - vertices are already in world space
		shader.setMat4("model", glm::mat4(1.0f));
		shader.setMat4("view", view);
		shader.setMat4("projection", projection);
		shader.setFloat("uTessQualityMultiplier", tess_quality_multiplier);
		shader.setFloat("uTessLevelMax", 64.0f);
		shader.setFloat("uTessLevelMin", 1.0f);

		if (clip_plane) {
			shader.setVec4("clipPlane", *clip_plane);
		} else {
			shader.setVec4("clipPlane", glm::vec4(0, 0, 0, 0));
		}

		glBindVertexArray(vao_);
		glPatchParameteri(GL_PATCH_VERTICES, 4);

		// Build draw commands if dirty
		if (draw_commands_dirty_) {
			draw_commands_.clear();
			draw_commands_.reserve(chunk_allocations_.size());

			for (const auto& [key, alloc] : chunk_allocations_) {
				DrawCommand cmd{};
				cmd.count = static_cast<GLuint>(alloc.index_count);
				cmd.instanceCount = 1;
				cmd.firstIndex = static_cast<GLuint>(alloc.index_offset);
				cmd.baseVertex = static_cast<GLint>(alloc.vertex_offset);
				cmd.baseInstance = 0;
				draw_commands_.push_back(cmd);
			}

			// Upload draw commands to GPU buffer
			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, draw_command_buffer_);
			glBufferData(
				GL_DRAW_INDIRECT_BUFFER,
				draw_commands_.size() * sizeof(DrawCommand),
				draw_commands_.data(),
				GL_DYNAMIC_DRAW
			);

			draw_commands_dirty_ = false;
		}

		// Single API call renders all terrain chunks!
		// GPU reads draw parameters from the indirect buffer
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, draw_command_buffer_);
		glMultiDrawElementsIndirect(
			GL_PATCHES,
			GL_UNSIGNED_INT,
			nullptr,  // offset into indirect buffer
			static_cast<GLsizei>(draw_commands_.size()),
			sizeof(DrawCommand)
		);

		glBindVertexArray(0);
	}

	size_t TerrainRenderManager::GetRegisteredChunkCount() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return chunk_allocations_.size();
	}

	size_t TerrainRenderManager::GetTotalVertexCount() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return vertex_usage_;
	}

	size_t TerrainRenderManager::GetTotalIndexCount() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return index_usage_;
	}

} // namespace Boidsish
