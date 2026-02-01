#include "terrain_render_manager.h"

#include <algorithm>
#include <iostream>
#include <cstring>

#include "profiler.h"
#include "opengl_helpers.h"
#include "graphics.h" // For Frustum
#include <shader.h>

namespace Boidsish {

	TerrainRenderManager::TerrainRenderManager(int chunk_size, int max_chunks):
		chunk_size_(chunk_size), max_chunks_(max_chunks), heightmap_resolution_(chunk_size + 1) {

		instance_vbo_ring_ = std::make_unique<PersistentRingBuffer>(GL_ARRAY_BUFFER, max_chunks * sizeof(InstanceData));
		instance_buffer_capacity_ = max_chunks * sizeof(InstanceData);

		CreateGridMesh();
		EnsureTextureCapacity(max_chunks);
	}

	TerrainRenderManager::~TerrainRenderManager() {
		if (grid_vao_)
			glDeleteVertexArrays(1, &grid_vao_);
		if (grid_vbo_)
			glDeleteBuffers(1, &grid_vbo_);
		if (grid_ebo_)
			glDeleteBuffers(1, &grid_ebo_);
		instance_vbo_ring_.reset();
		if (heightmap_texture_)
			glDeleteTextures(1, &heightmap_texture_);
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

		// Set up instance attributes (from instance_vbo_ ring created in constructor)
		glBindBuffer(GL_ARRAY_BUFFER, instance_vbo_ring_->GetVBO());

		// Instance attribute: world_offset_and_slice (location 3)
		glVertexAttribPointer(
			3,
			4,
			GL_FLOAT,
			GL_FALSE,
			sizeof(InstanceData),
			(void*)offsetof(InstanceData, world_offset_and_slice)
		);
		glEnableVertexAttribArray(3);
		glVertexAttribDivisor(3, 1); // Per-instance

		// Instance attribute: bounds (location 4)
		glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)offsetof(InstanceData, bounds));
		glEnableVertexAttribArray(4);
		glVertexAttribDivisor(4, 1); // Per-instance

		glBindVertexArray(0);
	}

	void TerrainRenderManager::EnsureTextureCapacity(int required_slices) {
		if (heightmap_texture_ && required_slices <= max_chunks_) {
			return; // Already have enough capacity
		}

		// Query GPU limit for texture array layers
		GLint max_layers = 0;
		glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &max_layers);
		if (max_layers <= 0)
			max_layers = 512; // Fallback

		// Clamp to GPU's maximum supported layers
		int new_capacity = std::max(max_chunks_, required_slices);
		if (new_capacity > max_layers) {
			new_capacity = max_layers;
		}

		// If already at max capacity, nothing to do
		if (heightmap_texture_ && new_capacity <= max_chunks_) {
			return;
		}

		// If we need to resize and texture exists, existing data will be lost
		// This shouldn't happen often with proper capacity management
		if (heightmap_texture_) {
			std::cerr << "[TerrainRenderManager] WARNING: Texture array resize from " << max_chunks_ << " to "
					  << new_capacity << " - existing heightmap data will be lost!" << std::endl;
			glDeleteTextures(1, &heightmap_texture_);
			heightmap_texture_ = 0;
			// Reset slice tracking since all data is lost
			next_slice_ = 0;
			free_slices_.clear();
			chunks_.clear();
		}

		max_chunks_ = new_capacity;

		// Create 2D texture array for heightmaps
		// Format: RGBA16F - R=height, GBA=normal.xyz
		glGenTextures(1, &heightmap_texture_);
		glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture_);

		// Allocate storage for all slices
		glTexImage3D(
			GL_TEXTURE_2D_ARRAY,
			0,                     // mip level
			GL_RGBA16F,            // internal format (height + normal)
			heightmap_resolution_, // width
			heightmap_resolution_, // height
			max_chunks_,           // depth (number of slices)
			0,                     // border
			GL_RGBA,               // format
			GL_FLOAT,              // type
			nullptr                // no initial data
		);

		// Check for errors
		GLenum err = glGetError();
		if (err != GL_NO_ERROR) {
			std::cerr << "[TerrainRenderManager] ERROR: glTexImage3D failed with error " << err
					  << " (resolution=" << heightmap_resolution_ << ", slices=" << max_chunks_ << ")" << std::endl;
		}

		// Filtering for smooth interpolation
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	}

	void TerrainRenderManager::UploadHeightmapSlice(
		int                           slice,
		const std::vector<float>&     heightmap,
		const std::vector<glm::vec3>& normals
	) {
		const int num_pixels = heightmap_resolution_ * heightmap_resolution_;

		// Pack height + normal into RGBA16F format
		std::vector<float> packed_data;
		packed_data.reserve(num_pixels * 4);

		for (int i = 0; i < num_pixels; ++i) {
			packed_data.push_back(heightmap[i]); // R = height
			packed_data.push_back(normals[i].x); // G = normal.x
			packed_data.push_back(normals[i].y); // B = normal.y
			packed_data.push_back(normals[i].z); // A = normal.z
		}

		glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture_);
		glTexSubImage3D(
			GL_TEXTURE_2D_ARRAY,
			0, // mip level
			0,
			0,
			slice,                 // x, y, z offset
			heightmap_resolution_, // width
			heightmap_resolution_, // height
			1,                     // depth (one slice)
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
		const std::vector<unsigned int>& indices, // Not used in this implementation
		float                            min_y,
		float                            max_y,
		const glm::vec3&                 world_offset
	) {
		// Deferred eviction callback to avoid deadlock
		// (caller may hold terrain generator's mutex, and callback needs that mutex)
		bool                should_notify_eviction = false;
		std::pair<int, int> evicted_chunk_key;

		const int              res = heightmap_resolution_;
		std::vector<float>     heightmap(res * res);
		std::vector<glm::vec3> reordered_normals(res * res);

		for (int x = 0; x < res; ++x) {
			for (int z = 0; z < res; ++z) {
				int src_idx = x * res + z; // X-major (how terrain generator stores it)
				int dst_idx = z * res + x; // Z-major / row-major (for texture)

				heightmap[dst_idx] = positions[src_idx].y;
				reordered_normals[dst_idx] = normals[src_idx];
			}
		}

		// Scoped lock - released before calling eviction callback to avoid deadlock
		{
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
					// Check if we can grow the texture array
					GLint max_layers = 0;
					glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &max_layers);

					if (max_chunks_ >= max_layers) {
						// At GPU capacity - use LRU eviction based on distance from camera
						// Use last known camera position for eviction decisions
						glm::vec2 camera_pos_2d(last_camera_pos_.x, last_camera_pos_.z);

						float               max_dist_sq = -1.0f;
						std::pair<int, int> farthest_key;

						for (const auto& [key, chunk] : chunks_) {
							glm::vec2 chunk_center(
								chunk.world_offset.x + chunk_size_ * 0.5f,
								chunk.world_offset.y + chunk_size_ * 0.5f
							);
							float dist_sq = glm::dot(chunk_center - camera_pos_2d, chunk_center - camera_pos_2d);
							if (dist_sq > max_dist_sq) {
								max_dist_sq = dist_sq;
								farthest_key = key;
							}
						}

						if (max_dist_sq >= 0) {
							// Evict the farthest chunk and reuse its slice
							auto evict_it = chunks_.find(farthest_key);
							if (evict_it != chunks_.end()) {
								slice = evict_it->second.texture_slice;
								chunks_.erase(evict_it);
								evicted_chunk_key = farthest_key;
								should_notify_eviction = true;
							} else {
								return; // Shouldn't happen
							}
						} else {
							return; // No chunks to evict
						}
					} else {
						// Can grow, but cap at GPU limit
						int new_capacity = std::min(max_chunks_ * 2, max_layers);
						EnsureTextureCapacity(new_capacity);
						slice = next_slice_++;
					}
				} else {
					slice = next_slice_++;
				}
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
		} // mutex released here

		// Call eviction callback outside the lock to avoid deadlock
		if (should_notify_eviction && eviction_callback_) {
			eviction_callback_(evicted_chunk_key);
		}
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
		glm::vec3 min_corner(chunk.world_offset.x, chunk.min_y, chunk.world_offset.y);
		glm::vec3 max_corner(chunk.world_offset.x + chunk_size_, chunk.max_y, chunk.world_offset.y + chunk_size_);

		glm::vec3 center = (min_corner + max_corner) * 0.5f;
		glm::vec3 half_size = (max_corner - min_corner) * 0.5f;

		// Test against all 6 frustum planes
		for (int i = 0; i < 6; ++i) {
			// Compute the "positive vertex" distance
			float r = half_size.x * std::abs(frustum.planes[i].normal.x) +
				half_size.y * std::abs(frustum.planes[i].normal.y) + half_size.z * std::abs(frustum.planes[i].normal.z);

			float d = glm::dot(center, frustum.planes[i].normal) + frustum.planes[i].distance;

			if (d < -r) {
				return false; // Completely outside this plane
			}
		}

		return true; // Inside or intersecting all planes
	}

	void TerrainRenderManager::PrepareForRender(const Frustum& frustum, const glm::vec3& camera_pos) {
		std::lock_guard<std::mutex> lock(mutex_);

		// Store camera position for LRU eviction decisions in RegisterChunk
		last_camera_pos_ = camera_pos;

		visible_instances_.clear();
		visible_instances_.reserve(chunks_.size());

		// Collect visible chunks with distance info
		struct VisibleChunk {
			InstanceData instance;
			float        distance_sq;
		};

		std::vector<VisibleChunk> visible_chunks;
		visible_chunks.reserve(chunks_.size());

		glm::vec2 camera_pos_2d(camera_pos.x, camera_pos.z);

		for (const auto& [key, chunk] : chunks_) {
			if (IsChunkVisible(chunk, frustum)) {
				InstanceData instance{};
				instance.world_offset_and_slice = glm::vec4(
					chunk.world_offset.x,
					0.0f,                 // Y offset is always 0 (height comes from heightmap)
					chunk.world_offset.y, // This is the Z world coordinate
					static_cast<float>(chunk.texture_slice)
				);
				instance.bounds = glm::vec4(chunk.min_y, chunk.max_y, 0.0f, 0.0f);

				// Calculate distance from chunk center to camera
				glm::vec2 chunk_center(
					chunk.world_offset.x + chunk_size_ * 0.5f,
					chunk.world_offset.y + chunk_size_ * 0.5f
				);
				float dist_sq = glm::dot(chunk_center - camera_pos_2d, chunk_center - camera_pos_2d);

				visible_chunks.push_back({instance, dist_sq});
			}
		}

		// Sort by distance (front-to-back for better early-Z rejection)
		std::sort(visible_chunks.begin(), visible_chunks.end(), [](const VisibleChunk& a, const VisibleChunk& b) {
			return a.distance_sq < b.distance_sq;
		});

		// Build final instance list
		for (const auto& vc : visible_chunks) {
			visible_instances_.push_back(vc.instance);
		}

		// Upload instance data to GPU
		if (!visible_instances_.empty()) {
			size_t required_size = visible_instances_.size() * sizeof(InstanceData);

			instance_vbo_ring_->EnsureCapacity(required_size);

			// If buffer was reallocated, we need to re-setup VAO
			static GLuint last_vbo = 0;
			if (instance_vbo_ring_->GetVBO() != last_vbo) {
				glBindVertexArray(grid_vao_);
				glBindBuffer(GL_ARRAY_BUFFER, instance_vbo_ring_->GetVBO());

				glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)offsetof(InstanceData, world_offset_and_slice));
				glEnableVertexAttribArray(3);
				glVertexAttribDivisor(3, 1);

				glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)offsetof(InstanceData, bounds));
				glEnableVertexAttribArray(4);
				glVertexAttribDivisor(4, 1);

				glBindVertexArray(0);
				last_vbo = instance_vbo_ring_->GetVBO();
			}

			void* ptr = instance_vbo_ring_->GetCurrentPtr();
			if (ptr) {
				memcpy(ptr, visible_instances_.data(), required_size);
			}
		}
	}

	void TerrainRenderManager::Render(
		Shader&                         shader,
		const glm::mat4&                view,
		const glm::mat4&                projection,
		const std::optional<glm::vec4>& clip_plane,
		float                           tess_quality_multiplier
	) {
		PROJECT_PROFILE_SCOPE("TerrainRenderManager::Render");
		std::lock_guard<std::mutex> lock(mutex_);

		if (visible_instances_.empty() || grid_vao_ == 0 || grid_index_count_ == 0 || !instance_vbo_ring_) {
			return;
		}

		shader.use();
		shader.setMat4("view", view);
		shader.setMat4("projection", projection);
		shader.setMat4("model", glm::mat4(1.0f)); // Identity - instances provide world offset
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

		// Bind VAO
		glBindVertexArray(grid_vao_);

		// Update attribute pointers for the current ring buffer offset
		size_t offset = instance_vbo_ring_->GetOffset();
		glBindBuffer(GL_ARRAY_BUFFER, instance_vbo_ring_->GetVBO());
		glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)(offset + offsetof(InstanceData, world_offset_and_slice)));
		glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)(offset + offsetof(InstanceData, bounds)));

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

		instance_vbo_ring_->AdvanceFrame();
	}

	size_t TerrainRenderManager::GetRegisteredChunkCount() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return chunks_.size();
	}

	size_t TerrainRenderManager::GetVisibleChunkCount() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return visible_instances_.size();
	}

	std::vector<glm::vec4> TerrainRenderManager::GetChunkInfo() const {
		std::lock_guard<std::mutex> lock(mutex_);
		std::vector<glm::vec4>      result;
		result.reserve(chunks_.size());
		for (const auto& [key, chunk] : chunks_) {
			result.push_back(
				glm::vec4(
					chunk.world_offset.x, // x world offset
					chunk.world_offset.y, // z world offset (stored as y in vec2)
					static_cast<float>(chunk.texture_slice),
					static_cast<float>(chunk_size_)
				)
			);
		}
		return result;
	}

} // namespace Boidsish
