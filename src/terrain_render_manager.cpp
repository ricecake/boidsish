#include "terrain_render_manager.h"

#include <algorithm>
#include <iostream>

#include "biome_properties.h"
#include "constants.h"
#include "graphics.h" // For Frustum
#include <shader.h>

namespace Boidsish {

	TerrainRenderManager::TerrainRenderManager(int chunk_size, int max_chunks):
		chunk_size_(chunk_size), max_chunks_(max_chunks), heightmap_resolution_(chunk_size + 1) {
		// Create Biome UBO
		glGenBuffers(1, &biome_ubo_);
		glBindBuffer(GL_UNIFORM_BUFFER, biome_ubo_);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(BiomeShaderProperties) * kBiomes.size(), nullptr, GL_STATIC_DRAW);

		std::vector<BiomeShaderProperties> shader_biomes;
		for (const auto& b : kBiomes) {
			BiomeShaderProperties sb;
			sb.albedo_roughness = glm::vec4(b.albedo, b.roughness);
			sb.params = glm::vec4(b.metallic, b.detailStrength, b.detailScale, 0.0f);
			shader_biomes.push_back(sb);
		}
		glBufferSubData(
			GL_UNIFORM_BUFFER,
			0,
			sizeof(BiomeShaderProperties) * shader_biomes.size(),
			shader_biomes.data()
		);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

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
		if (grid_vao_)
			glDeleteVertexArrays(1, &grid_vao_);
		if (grid_vbo_)
			glDeleteBuffers(1, &grid_vbo_);
		if (grid_ebo_)
			glDeleteBuffers(1, &grid_ebo_);
		if (instance_vbo_)
			glDeleteBuffers(1, &instance_vbo_);
		if (heightmap_texture_)
			glDeleteTextures(1, &heightmap_texture_);
		if (biome_texture_)
			glDeleteTextures(1, &biome_texture_);
		if (biome_ubo_)
			glDeleteBuffers(1, &biome_ubo_);
	}

	void TerrainRenderManager::CreateGridMesh() {
		// Create a single quad for one chunk.
		// The tessellation shader will subdivide it according to need.
		// This avoids having a minimum tessellation level above what may be strictly needed.

		// Vertex data: position (x, y, z) + texcoord (u, v) = 5 floats per vertex
		// Corners: (0,0), (chunk_size, 0), (chunk_size, chunk_size), (0, chunk_size)
		std::vector<float> vertices = {
			0.0f,
			0.0f,
			0.0f,
			0.0f,
			0.0f, // Top-left
			(float)chunk_size_,
			0.0f,
			0.0f,
			1.0f,
			0.0f, // Top-right
			(float)chunk_size_,
			0.0f,
			(float)chunk_size_,
			1.0f,
			1.0f, // Bottom-right
			0.0f,
			0.0f,
			(float)chunk_size_,
			0.0f,
			1.0f // Bottom-left
		};

		// Indices for a single quad patch
		std::vector<unsigned int> indices = {0, 1, 2, 3};

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
		if (heightmap_texture_ && biome_texture_ && required_slices <= max_chunks_) {
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

			if (biome_texture_) {
				glDeleteTextures(1, &biome_texture_);
				biome_texture_ = 0;
			}

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

		// Create 2D texture array for biomes
		// Format: RG8 - R=low_idx, G=t
		glGenTextures(1, &biome_texture_);
		glBindTexture(GL_TEXTURE_2D_ARRAY, biome_texture_);

		glTexImage3D(
			GL_TEXTURE_2D_ARRAY,
			0,                     // mip level
			GL_RG8,                // internal format
			heightmap_resolution_, // width
			heightmap_resolution_, // height
			max_chunks_,           // depth (number of slices)
			0,                     // border
			GL_RG,                 // format
			GL_UNSIGNED_BYTE,      // type
			nullptr                // no initial data
		);

		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	}

	void TerrainRenderManager::UploadHeightmapSlice(
		int                           slice,
		const std::vector<float>&     heightmap,
		const std::vector<glm::vec3>& normals,
		const std::vector<glm::vec2>& biomes
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

		// Pack biome indices/weights into RG8 format
		std::vector<uint8_t> biome_data;
		biome_data.reserve(num_pixels * 2);
		for (int i = 0; i < num_pixels; ++i) {
			biome_data.push_back(static_cast<uint8_t>(biomes[i].x));                 // R = low_idx
			biome_data.push_back(static_cast<uint8_t>(biomes[i].y * 255.0f + 0.5f)); // G = t
		}

		glBindTexture(GL_TEXTURE_2D_ARRAY, biome_texture_);
		glTexSubImage3D(
			GL_TEXTURE_2D_ARRAY,
			0, // mip level
			0,
			0,
			slice,
			heightmap_resolution_,
			heightmap_resolution_,
			1,
			GL_RG,
			GL_UNSIGNED_BYTE,
			biome_data.data()
		);

		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	}

	void TerrainRenderManager::RegisterChunk(
		std::pair<int, int>              chunk_key,
		const std::vector<glm::vec3>&    positions,
		const std::vector<glm::vec3>&    normals,
		const std::vector<glm::vec2>&    biomes,
		const std::vector<unsigned int>& indices, // Not used in this implementation
		float                            min_y,
		float                            max_y,
		const glm::vec3&                 world_offset
	) {
		// Deferred eviction callback to avoid deadlock
		// (caller may hold terrain generator's mutex, and callback needs that mutex)
		bool                should_notify_eviction = false;
		std::pair<int, int> evicted_chunk_key;

		// The positions array from TerrainGenerator is in X-major order:
		//   positions[x * num_z + z] = position at local (x, y, z)
		// But OpenGL textures are row-major (Y/V axis is rows), so we need:
		//   texture[z * num_x + x] = height at local (x, z)
		// This means we need to transpose the data.

		const int              res = heightmap_resolution_;
		std::vector<float>     heightmap(res * res);
		std::vector<glm::vec3> reordered_normals(res * res);
		std::vector<glm::vec2> reordered_biomes(res * res);

		for (int x = 0; x < res; ++x) {
			for (int z = 0; z < res; ++z) {
				int src_idx = x * res + z; // X-major (how terrain generator stores it)
				int dst_idx = z * res + x; // Z-major / row-major (for texture)

				heightmap[dst_idx] = positions[src_idx].y;
				reordered_normals[dst_idx] = normals[src_idx];
				reordered_biomes[dst_idx] = biomes[src_idx];
			}
		}

		// Scoped lock - released before calling eviction callback to avoid deadlock
		{
			std::lock_guard<std::mutex> lock(mutex_);

			// If chunk already exists, update it
			auto it = chunks_.find(chunk_key);
			if (it != chunks_.end()) {
				// Update existing chunk's heightmap
				UploadHeightmapSlice(it->second.texture_slice, heightmap, reordered_normals, reordered_biomes);
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
							float     scaled_chunk_size = chunk_size_ * last_world_scale_;
							glm::vec2 chunk_center(
								chunk.world_offset.x + scaled_chunk_size * 0.5f,
								chunk.world_offset.y + scaled_chunk_size * 0.5f
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
								// Queue callback to be called after we finish registration
								// (avoids deadlock if callback tries to acquire terrain generator's mutex)
								evicted_chunk_key = farthest_key;
								should_notify_eviction = true;
							} else {
								return; // Shouldn't happen, but safety check
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
			UploadHeightmapSlice(slice, heightmap, reordered_normals, reordered_biomes);

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

	bool TerrainRenderManager::IsChunkVisible(const ChunkInfo& chunk, const Frustum& frustum, float world_scale) const {
		// Build AABB for this chunk
		float     scaled_chunk_size = chunk_size_ * world_scale;
		glm::vec3 min_corner(chunk.world_offset.x, chunk.min_y, chunk.world_offset.y);
		glm::vec3 max_corner(
			chunk.world_offset.x + scaled_chunk_size,
			chunk.max_y,
			chunk.world_offset.y + scaled_chunk_size
		);

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

	void
	TerrainRenderManager::PrepareForRender(const Frustum& frustum, const glm::vec3& camera_pos, float world_scale) {
		std::lock_guard<std::mutex> lock(mutex_);

		// Store camera position and world scale for LRU eviction decisions in RegisterChunk
		last_camera_pos_ = camera_pos;
		last_world_scale_ = world_scale;

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
			if (IsChunkVisible(chunk, frustum, world_scale)) {
				InstanceData instance{};
				instance.world_offset_and_slice = glm::vec4(
					chunk.world_offset.x,
					0.0f,                 // Y offset is always 0 (height comes from heightmap)
					chunk.world_offset.y, // This is the Z world coordinate
					static_cast<float>(chunk.texture_slice)
				);
				instance.bounds = glm::vec4(chunk.min_y, chunk.max_y, 0.0f, 0.0f);

				// Calculate distance from chunk center to camera
				float     scaled_chunk_size = chunk_size_ * world_scale;
				glm::vec2 chunk_center(
					chunk.world_offset.x + scaled_chunk_size * 0.5f,
					chunk.world_offset.y + scaled_chunk_size * 0.5f
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
			glBindBuffer(GL_ARRAY_BUFFER, instance_vbo_);

			size_t required_size = visible_instances_.size() * sizeof(InstanceData);
			if (required_size > instance_buffer_capacity_) {
				// Grow buffer - need to re-bind VAO attributes after this!
				instance_buffer_capacity_ = required_size * 2;
				glBufferData(GL_ARRAY_BUFFER, instance_buffer_capacity_, nullptr, GL_DYNAMIC_DRAW);

				// Re-bind instance attributes to VAO since buffer was reallocated
				glBindVertexArray(grid_vao_);
				glBindBuffer(GL_ARRAY_BUFFER, instance_vbo_);

				glVertexAttribPointer(
					3,
					4,
					GL_FLOAT,
					GL_FALSE,
					sizeof(InstanceData),
					(void*)offsetof(InstanceData, world_offset_and_slice)
				);
				glEnableVertexAttribArray(3);
				glVertexAttribDivisor(3, 1);

				glVertexAttribPointer(
					4,
					4,
					GL_FLOAT,
					GL_FALSE,
					sizeof(InstanceData),
					(void*)offsetof(InstanceData, bounds)
				);
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
		const glm::vec2&                viewport_size,
		const std::optional<glm::vec4>& clip_plane,
		float                           tess_quality_multiplier
	) {
		std::lock_guard<std::mutex> lock(mutex_);

		if (visible_instances_.empty() || grid_vao_ == 0 || grid_index_count_ == 0) {
			return;
		}

		shader.use();
		shader.setMat4("view", view);
		shader.setMat4("projection", projection);
		shader.setVec2("uViewportSize", viewport_size);
		shader.setMat4("model", glm::mat4(1.0f)); // Identity - instances provide world offset
		shader.setFloat("uTessQualityMultiplier", tess_quality_multiplier);
		shader.setFloat("uTessLevelMax", 64.0f);
		shader.setFloat("uTessLevelMin", 1.0f);
		shader.setFloat("uChunkSize", chunk_size_ * last_world_scale_);

		if (clip_plane) {
			shader.setVec4("clipPlane", *clip_plane);
		} else {
			shader.setVec4("clipPlane", glm::vec4(0, 0, 0, 0));
		}

		// Bind heightmap texture array
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture_);
		shader.setInt("uHeightmap", 0);

		// Bind biome texture array
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D_ARRAY, biome_texture_);
		shader.setInt("uBiomeMap", 1);

		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_3D, noise_texture_);
		shader.setInt("u_noiseTexture", 2);

		glActiveTexture(GL_TEXTURE6);
		glBindTexture(GL_TEXTURE_3D, curl_texture_);
		shader.setInt("u_curlTexture", 3);

		// Bind Biome UBO
		glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::Biomes(), biome_ubo_);

		// Bind VAO (instance attributes already configured during initialization)
		glBindVertexArray(grid_vao_);

		// Note: EBO is already captured in VAO state during initialization

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

	std::vector<glm::vec4> TerrainRenderManager::GetChunkInfo(float world_scale) const {
		std::lock_guard<std::mutex> lock(mutex_);
		std::vector<glm::vec4>      result;
		result.reserve(chunks_.size());
		for (const auto& [key, chunk] : chunks_) {
			result.push_back(
				glm::vec4(
					chunk.world_offset.x, // x world offset
					chunk.world_offset.y, // z world offset (stored as y in vec2)
					static_cast<float>(chunk.texture_slice),
					static_cast<float>(chunk_size_ * world_scale)
				)
			);
		}
		return result;
	}

} // namespace Boidsish
