#include "terrain_render_manager.h"

#include <algorithm>
#include <iostream>

#include "graphics.h"  // For Frustum
#include <shader.h>

namespace Boidsish {

	TerrainRenderManager::TerrainRenderManager(int chunk_size, int max_chunks)
		: chunk_size_(chunk_size)
		, max_chunks_(max_chunks)
		, heightmap_resolution_(chunk_size + 1)
	{
		// Create instance buffer first so we can set up VAO attributes
		// Pre-allocate for max_chunks to avoid reallocation
		glGenBuffers(1, &instance_vbo_);
		glBindBuffer(GL_ARRAY_BUFFER, instance_vbo_);
		instance_buffer_capacity_ = max_chunks * sizeof(InstanceData);
		glBufferData(GL_ARRAY_BUFFER, instance_buffer_capacity_, nullptr, GL_DYNAMIC_DRAW);

		CreateGridMesh();
		EnsureTextureCapacity(max_chunks);
	}

	TerrainRenderManager::~TerrainRenderManager() {
		if (grid_vao_) glDeleteVertexArrays(1, &grid_vao_);
		if (grid_vbo_) glDeleteBuffers(1, &grid_vbo_);
		if (grid_ebo_) glDeleteBuffers(1, &grid_ebo_);
		if (instance_vbo_) glDeleteBuffers(1, &instance_vbo_);
		if (heightmap_texture_) glDeleteTextures(1, &heightmap_texture_);
	}

	void TerrainRenderManager::CreateGridMesh() {
		// Create a flat grid of quads for one chunk
		// Vertices are at integer coordinates [0, chunk_size] in XZ plane, Y=0
		// The tessellation/vertex shader will displace Y based on heightmap lookup

		const int num_verts = heightmap_resolution_ * heightmap_resolution_;
		const int num_quads = chunk_size_ * chunk_size_;

		// Vertex data: position (x, y, z) + texcoord (u, v) = 5 floats per vertex
		std::vector<float> vertices;
		vertices.reserve(num_verts * 5);

		for (int z = 0; z < heightmap_resolution_; ++z) {
			for (int x = 0; x < heightmap_resolution_; ++x) {
				// Position (flat grid, Y=0)
				vertices.push_back(static_cast<float>(x));
				vertices.push_back(0.0f);
				vertices.push_back(static_cast<float>(z));
				// Texcoord (normalized 0-1 for heightmap sampling)
				vertices.push_back(static_cast<float>(x) / chunk_size_);
				vertices.push_back(static_cast<float>(z) / chunk_size_);
			}
		}

		// Indices for quads (4 vertices per quad for GL_PATCHES)
		std::vector<unsigned int> indices;
		indices.reserve(num_quads * 4);

		for (int z = 0; z < chunk_size_; ++z) {
			for (int x = 0; x < chunk_size_; ++x) {
				int i0 = z * heightmap_resolution_ + x;
				int i1 = z * heightmap_resolution_ + (x + 1);
				int i2 = (z + 1) * heightmap_resolution_ + (x + 1);
				int i3 = (z + 1) * heightmap_resolution_ + x;
				indices.push_back(i0);
				indices.push_back(i1);
				indices.push_back(i2);
				indices.push_back(i3);
			}
		}

		grid_index_count_ = indices.size();

		// Create VAO
		glGenVertexArrays(1, &grid_vao_);
		glBindVertexArray(grid_vao_);

		// Vertex buffer
		glGenBuffers(1, &grid_vbo_);
		glBindBuffer(GL_ARRAY_BUFFER, grid_vbo_);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

		// Position attribute (location 0)
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);

		// Texcoord attribute (location 1) - will be used for heightmap sampling
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);

		// Index buffer
		glGenBuffers(1, &grid_ebo_);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, grid_ebo_);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

		// Set up instance attributes (from instance_vbo_ created in constructor)
		glBindBuffer(GL_ARRAY_BUFFER, instance_vbo_);

		// Instance attribute: world_offset_and_slice (location 3)
		glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData),
		                      (void*)offsetof(InstanceData, world_offset_and_slice));
		glEnableVertexAttribArray(3);
		glVertexAttribDivisor(3, 1);  // Per-instance

		// Instance attribute: bounds (location 4)
		glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData),
		                      (void*)offsetof(InstanceData, bounds));
		glEnableVertexAttribArray(4);
		glVertexAttribDivisor(4, 1);  // Per-instance

		glBindVertexArray(0);
	}

	void TerrainRenderManager::EnsureTextureCapacity(int required_slices) {
		if (heightmap_texture_ && required_slices <= max_chunks_) {
			return;  // Already have enough capacity
		}

		if (heightmap_texture_) {
			glDeleteTextures(1, &heightmap_texture_);
		}

		max_chunks_ = std::max(max_chunks_, required_slices);

		// Create 2D texture array for heightmaps
		// Format: RG32F - R = height, G = packed normal (or use RGBA for full normal)
		// Actually, let's use RGBA16F: R=height, GBA=normal.xyz
		glGenTextures(1, &heightmap_texture_);
		glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture_);

		// Allocate storage for all slices
		glTexImage3D(
			GL_TEXTURE_2D_ARRAY,
			0,                          // mip level
			GL_RGBA16F,                 // internal format (height + normal)
			heightmap_resolution_,      // width
			heightmap_resolution_,      // height
			max_chunks_,                // depth (number of slices)
			0,                          // border
			GL_RGBA,                    // format
			GL_FLOAT,                   // type
			nullptr                     // no initial data
		);

		// Filtering for smooth interpolation
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	}

	void TerrainRenderManager::UploadHeightmapSlice(
		int slice,
		const std::vector<float>& heightmap,
		const std::vector<glm::vec3>& normals
	) {
		const int num_pixels = heightmap_resolution_ * heightmap_resolution_;

		// Pack height + normal into RGBA16F format
		std::vector<float> packed_data;
		packed_data.reserve(num_pixels * 4);

		for (int i = 0; i < num_pixels; ++i) {
			packed_data.push_back(heightmap[i]);        // R = height
			packed_data.push_back(normals[i].x);        // G = normal.x
			packed_data.push_back(normals[i].y);        // B = normal.y
			packed_data.push_back(normals[i].z);        // A = normal.z
		}

		glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture_);
		glTexSubImage3D(
			GL_TEXTURE_2D_ARRAY,
			0,                          // mip level
			0, 0, slice,                // x, y, z offset
			heightmap_resolution_,      // width
			heightmap_resolution_,      // height
			1,                          // depth (one slice)
			GL_RGBA,
			GL_FLOAT,
			packed_data.data()
		);
		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	}

	void TerrainRenderManager::RegisterChunk(
		std::pair<int, int>              chunk_key,
		const std::vector<glm::vec3>&    positions,
		const std::vector<glm::vec3>&    normals,
		const std::vector<unsigned int>& indices,  // Not used in this implementation
		float                            min_y,
		float                            max_y,
		const glm::vec3&                 world_offset
	) {
		// The positions array from TerrainGenerator is in X-major order:
		//   positions[x * num_z + z] = position at local (x, y, z)
		// But OpenGL textures are row-major (Y/V axis is rows), so we need:
		//   texture[z * num_x + x] = height at local (x, z)
		// This means we need to transpose the data.

		const int res = heightmap_resolution_;
		std::vector<float> heightmap(res * res);
		std::vector<glm::vec3> reordered_normals(res * res);

		for (int x = 0; x < res; ++x) {
			for (int z = 0; z < res; ++z) {
				int src_idx = x * res + z;   // X-major (how terrain generator stores it)
				int dst_idx = z * res + x;   // Z-major / row-major (for texture)

				heightmap[dst_idx] = positions[src_idx].y;
				reordered_normals[dst_idx] = normals[src_idx];
			}
		}

		std::lock_guard<std::mutex> lock(mutex_);

		// If chunk already exists, update it
		auto it = chunks_.find(chunk_key);
		if (it != chunks_.end()) {
			// Update existing chunk's heightmap
			UploadHeightmapSlice(it->second.texture_slice, heightmap, reordered_normals);
			it->second.min_y = min_y;
			it->second.max_y = max_y;
			return;
		}

		// Allocate a texture slice
		int slice;
		if (!free_slices_.empty()) {
			slice = free_slices_.back();
			free_slices_.pop_back();
		} else {
			if (next_slice_ >= max_chunks_) {
				// Need to grow texture array
				EnsureTextureCapacity(max_chunks_ * 2);
			}
			slice = next_slice_++;
		}

		// Upload heightmap data
		UploadHeightmapSlice(slice, heightmap, reordered_normals);

		// Store chunk info
		ChunkInfo info{};
		info.texture_slice = slice;
		info.min_y = min_y;
		info.max_y = max_y;
		info.world_offset = glm::vec2(world_offset.x, world_offset.z);

		chunks_[chunk_key] = info;
	}

	void TerrainRenderManager::UnregisterChunk(std::pair<int, int> chunk_key) {
		std::lock_guard<std::mutex> lock(mutex_);

		auto it = chunks_.find(chunk_key);
		if (it == chunks_.end()) {
			return;
		}

		// Return slice to free list
		free_slices_.push_back(it->second.texture_slice);
		chunks_.erase(it);
	}

	bool TerrainRenderManager::HasChunk(std::pair<int, int> chunk_key) const {
		std::lock_guard<std::mutex> lock(mutex_);
		return chunks_.count(chunk_key) > 0;
	}

	bool TerrainRenderManager::IsChunkVisible(const ChunkInfo& chunk, const Frustum& frustum) const {
		// Build AABB for this chunk
		glm::vec3 min_corner(
			chunk.world_offset.x,
			chunk.min_y,
			chunk.world_offset.y
		);
		glm::vec3 max_corner(
			chunk.world_offset.x + chunk_size_,
			chunk.max_y,
			chunk.world_offset.y + chunk_size_
		);

		glm::vec3 center = (min_corner + max_corner) * 0.5f;
		glm::vec3 half_size = (max_corner - min_corner) * 0.5f;

		// Test against all 6 frustum planes
		for (int i = 0; i < 6; ++i) {
			// Compute the "positive vertex" distance
			float r = half_size.x * std::abs(frustum.planes[i].normal.x) +
			          half_size.y * std::abs(frustum.planes[i].normal.y) +
			          half_size.z * std::abs(frustum.planes[i].normal.z);

			float d = glm::dot(center, frustum.planes[i].normal) + frustum.planes[i].distance;

			if (d < -r) {
				return false;  // Completely outside this plane
			}
		}

		return true;  // Inside or intersecting all planes
	}

	void TerrainRenderManager::PrepareForRender(const Frustum& frustum, const glm::vec3& camera_pos) {
		std::lock_guard<std::mutex> lock(mutex_);

		visible_instances_.clear();
		visible_instances_.reserve(chunks_.size());

		for (const auto& [key, chunk] : chunks_) {
			// Always include all registered chunks - no frustum culling for now
			// to debug the rendering issue
			InstanceData instance{};
			instance.world_offset_and_slice = glm::vec4(
				chunk.world_offset.x,
				0.0f,  // Y offset is always 0 (height comes from heightmap)
				chunk.world_offset.y,  // This is the Z world coordinate
				static_cast<float>(chunk.texture_slice)
			);
			instance.bounds = glm::vec4(chunk.min_y, chunk.max_y, 0.0f, 0.0f);

			visible_instances_.push_back(instance);
		}

		// Upload instance data to GPU
		if (!visible_instances_.empty()) {
			glBindBuffer(GL_ARRAY_BUFFER, instance_vbo_);

			size_t required_size = visible_instances_.size() * sizeof(InstanceData);
			if (required_size > instance_buffer_capacity_) {
				// Grow buffer - need to re-bind VAO attributes after this!
				instance_buffer_capacity_ = required_size * 2;
				glBufferData(GL_ARRAY_BUFFER, instance_buffer_capacity_, nullptr, GL_DYNAMIC_DRAW);

				// Re-bind instance attributes to VAO since buffer was reallocated
				glBindVertexArray(grid_vao_);
				glBindBuffer(GL_ARRAY_BUFFER, instance_vbo_);

				glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData),
				                      (void*)offsetof(InstanceData, world_offset_and_slice));
				glEnableVertexAttribArray(3);
				glVertexAttribDivisor(3, 1);

				glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData),
				                      (void*)offsetof(InstanceData, bounds));
				glEnableVertexAttribArray(4);
				glVertexAttribDivisor(4, 1);

				glBindVertexArray(0);
			}

			glBufferSubData(GL_ARRAY_BUFFER, 0, required_size, visible_instances_.data());
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
	}

	void TerrainRenderManager::Render(
		Shader&                         shader,
		const glm::mat4&                view,
		const glm::mat4&                projection,
		const std::optional<glm::vec4>& clip_plane,
		float                           tess_quality_multiplier
	) {
		std::lock_guard<std::mutex> lock(mutex_);

		if (visible_instances_.empty()) {
			return;
		}

		shader.use();
		shader.setMat4("view", view);
		shader.setMat4("projection", projection);
		shader.setMat4("model", glm::mat4(1.0f));  // Identity - instances provide world offset
		shader.setFloat("uTessQualityMultiplier", tess_quality_multiplier);
		shader.setFloat("uTessLevelMax", 64.0f);
		shader.setFloat("uTessLevelMin", 1.0f);
		shader.setInt("uChunkSize", chunk_size_);

		if (clip_plane) {
			shader.setVec4("clipPlane", *clip_plane);
		} else {
			shader.setVec4("clipPlane", glm::vec4(0, 0, 0, 0));
		}

		// Bind heightmap texture array
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture_);
		shader.setInt("uHeightmap", 0);

		// Bind VAO (instance attributes already configured during initialization)
		glBindVertexArray(grid_vao_);

		// Set patch vertices for tessellation
		glPatchParameteri(GL_PATCH_VERTICES, 4);

		// Single instanced draw call for all visible chunks!
		glDrawElementsInstanced(
			GL_PATCHES,
			static_cast<GLsizei>(grid_index_count_),
			GL_UNSIGNED_INT,
			nullptr,
			static_cast<GLsizei>(visible_instances_.size())
		);

		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	}

	size_t TerrainRenderManager::GetRegisteredChunkCount() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return chunks_.size();
	}

	size_t TerrainRenderManager::GetVisibleChunkCount() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return visible_instances_.size();
	}

} // namespace Boidsish
