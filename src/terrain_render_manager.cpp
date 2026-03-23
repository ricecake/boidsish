#include "terrain_render_manager.h"

#include <algorithm>
#include <iostream>

#include "biome_properties.h"
#include "constants.h"
#include "graphics.h" // For Frustum
#include "shader.h"

namespace Boidsish {

	struct TerrainDataUbo {
		glm::ivec4 origin_size;    // x, z, size, is_bound (1)
		glm::vec4  terrain_params; // chunk_size, world_scale, unused, unused
	};

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

		// Create TerrainData UBO
		glGenBuffers(1, &terrain_data_ubo_);
		glBindBuffer(GL_UNIFORM_BUFFER, terrain_data_ubo_);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(TerrainDataUbo), nullptr, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		// Global terrain grid resources
		int grid_size = Constants::Class::Terrain::SliceMapSize();
		glGenTextures(1, &chunk_grid_texture_);
		glBindTexture(GL_TEXTURE_2D, chunk_grid_texture_);
		glTexStorage2D(GL_TEXTURE_2D, 1, GL_R16I, grid_size, grid_size);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glGenTextures(1, &max_height_grid_texture_);
		glBindTexture(GL_TEXTURE_2D, max_height_grid_texture_);
		int mips = 1 + static_cast<int>(std::floor(std::log2(grid_size)));
		glTexStorage2D(GL_TEXTURE_2D, mips, GL_R32F, grid_size, grid_size);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		grid_mip_shader_ = std::make_unique<ComputeShader>("shaders/terrain_hiz_generate.comp");
		mesh_gen_shader_ = std::make_unique<ComputeShader>("shaders/terrain_mesh_gen.comp");
		cull_shader_ = std::make_unique<ComputeShader>("shaders/terrain_cull.comp");

		// Create SSBOs
		glGenBuffers(1, &terrain_vertices_ssbo_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, terrain_vertices_ssbo_);
		vertices_per_chunk_ = heightmap_resolution_ * heightmap_resolution_;
		glBufferStorage(
			GL_SHADER_STORAGE_BUFFER,
			max_chunks * vertices_per_chunk_ * sizeof(TerrainVertex),
			nullptr,
			0
		);

		glGenBuffers(1, &terrain_metadata_ssbo_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, terrain_metadata_ssbo_);
		glBufferStorage(GL_SHADER_STORAGE_BUFFER, max_chunks * sizeof(ChunkMetadata), nullptr, GL_DYNAMIC_STORAGE_BIT);

		glGenBuffers(1, &terrain_indirect_args_ssbo_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, terrain_indirect_args_ssbo_);
		glBufferStorage(
			GL_SHADER_STORAGE_BUFFER,
			max_chunks * sizeof(DrawElementsIndirectCommand),
			nullptr,
			GL_DYNAMIC_STORAGE_BIT
		);

		glGenBuffers(1, &terrain_command_counter_buffer_);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, terrain_command_counter_buffer_);
		glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);

		CreateGridMesh();
		EnsureTextureCapacity(max_chunks);
	}

	TerrainRenderManager::~TerrainRenderManager() {
		if (terrain_vao_)
			glDeleteVertexArrays(1, &terrain_vao_);
		if (terrain_ebo_)
			glDeleteBuffers(1, &terrain_ebo_);
		if (terrain_vertices_ssbo_)
			glDeleteBuffers(1, &terrain_vertices_ssbo_);
		if (terrain_metadata_ssbo_)
			glDeleteBuffers(1, &terrain_metadata_ssbo_);
		if (terrain_indirect_args_ssbo_)
			glDeleteBuffers(1, &terrain_indirect_args_ssbo_);
		if (terrain_command_counter_buffer_)
			glDeleteBuffers(1, &terrain_command_counter_buffer_);

		if (heightmap_texture_)
			glDeleteTextures(1, &heightmap_texture_);
		if (biome_texture_)
			glDeleteTextures(1, &biome_texture_);
		if (biome_ubo_)
			glDeleteBuffers(1, &biome_ubo_);
		if (chunk_grid_texture_)
			glDeleteTextures(1, &chunk_grid_texture_);
		if (max_height_grid_texture_)
			glDeleteTextures(1, &max_height_grid_texture_);
		if (terrain_data_ubo_)
			glDeleteBuffers(1, &terrain_data_ubo_);
	}

	void TerrainRenderManager::CreateGridMesh() {
		// Create a shared index buffer for a 128x128 quad grid (chunk_size_ x chunk_size_)
		std::vector<unsigned int> indices;
		indices.reserve(chunk_size_ * chunk_size_ * 6);

		int res = heightmap_resolution_; // chunk_size_ + 1
		for (int z = 0; z < chunk_size_; ++z) {
			for (int x = 0; x < chunk_size_; ++x) {
				int i0 = z * res + x;
				int i1 = z * res + (x + 1);
				int i2 = (z + 1) * res + x;
				int i3 = (z + 1) * res + (x + 1);

				// Triangle 1
				indices.push_back(i0);
				indices.push_back(i2);
				indices.push_back(i1);

				// Triangle 2
				indices.push_back(i1);
				indices.push_back(i2);
				indices.push_back(i3);
			}
		}

		indices_per_chunk_ = indices.size();

		glGenVertexArrays(1, &terrain_vao_);
		glBindVertexArray(terrain_vao_);

		glGenBuffers(1, &terrain_ebo_);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, terrain_ebo_);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

		// Vertex data is fetched from SSBO in vertex shader using gl_VertexID,
		// so we don't need any glVertexAttribPointer calls.

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

			if (terrain_vertices_ssbo_)
				glDeleteBuffers(1, &terrain_vertices_ssbo_);
			if (terrain_metadata_ssbo_)
				glDeleteBuffers(1, &terrain_metadata_ssbo_);
			if (terrain_indirect_args_ssbo_)
				glDeleteBuffers(1, &terrain_indirect_args_ssbo_);
		}

		max_chunks_ = new_capacity;

		// Recreate SSBOs with new capacity
		glGenBuffers(1, &terrain_vertices_ssbo_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, terrain_vertices_ssbo_);
		glBufferStorage(
			GL_SHADER_STORAGE_BUFFER,
			max_chunks_ * vertices_per_chunk_ * sizeof(TerrainVertex),
			nullptr,
			0
		);

		glGenBuffers(1, &terrain_metadata_ssbo_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, terrain_metadata_ssbo_);
		glBufferStorage(GL_SHADER_STORAGE_BUFFER, max_chunks_ * sizeof(ChunkMetadata), nullptr, GL_DYNAMIC_STORAGE_BIT);

		glGenBuffers(1, &terrain_indirect_args_ssbo_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, terrain_indirect_args_ssbo_);
		glBufferStorage(
			GL_SHADER_STORAGE_BUFFER,
			max_chunks_ * sizeof(DrawElementsIndirectCommand),
			nullptr,
			GL_DYNAMIC_STORAGE_BIT
		);

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
				it->second.update_count++;
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

			// Update metadata SSBO and dispatch mesh generation
			ChunkMetadata metadata{};
			metadata.world_offset_and_slice = glm::vec4(world_offset.x, 0, world_offset.z, static_cast<float>(slice));
			metadata.bounds = glm::vec4(min_y, max_y, 0, 0);
			metadata.is_active = 1;

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, terrain_metadata_ssbo_);
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, slice * sizeof(ChunkMetadata), sizeof(ChunkMetadata), &metadata);

			// Dispatch mesh generation compute shader
			if (mesh_gen_shader_ && mesh_gen_shader_->isValid()) {
				mesh_gen_shader_->use();
				mesh_gen_shader_->setInt("u_sliceIndex", slice);
				mesh_gen_shader_->setVec3("u_worldOffset", world_offset);
				mesh_gen_shader_->setFloat("u_chunkSize", static_cast<float>(chunk_size_));
				mesh_gen_shader_->setInt("u_resolution", heightmap_resolution_);

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture_);
				mesh_gen_shader_->setInt("uHeightmap", 0);

				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D_ARRAY, biome_texture_);
				mesh_gen_shader_->setInt("uBiomeMap", 1);

				glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 40, terrain_vertices_ssbo_);

				glDispatchCompute((heightmap_resolution_ + 7) / 8, (heightmap_resolution_ + 7) / 8, 1);
				glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
			}
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

		int slice = it->second.texture_slice;

		// Deactivate in metadata SSBO
		ChunkMetadata metadata{};
		metadata.is_active = 0;
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, terrain_metadata_ssbo_);
		glBufferSubData(
			GL_SHADER_STORAGE_BUFFER,
			slice * sizeof(ChunkMetadata) + offsetof(ChunkMetadata, is_active),
			sizeof(uint32_t),
			&metadata.is_active
		);

		// Return slice to free list
		free_slices_.push_back(slice);
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

		UpdateGridTextures(world_scale);

		// Reset command counter
		uint32_t zero = 0;
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, terrain_command_counter_buffer_);
		glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(uint32_t), &zero);

		// Dispatch culling compute shader
		if (cull_shader_ && cull_shader_->isValid()) {
			cull_shader_->use();
			cull_shader_->setInt("u_maxChunks", max_chunks_);
			cull_shader_->setFloat("u_chunkSize", chunk_size_ * world_scale);
			cull_shader_->setUint("u_indicesPerChunk", static_cast<uint32_t>(indices_per_chunk_));
			cull_shader_->setUint("u_verticesPerChunk", static_cast<uint32_t>(vertices_per_chunk_));

			// Hi-Z occlusion culling uniforms
			cull_shader_->setBool("u_enableHiZ", enable_hiz_ && hiz_texture_ != 0);
			if (enable_hiz_ && hiz_texture_ != 0) {
				glActiveTexture(GL_TEXTURE15);
				glBindTexture(GL_TEXTURE_2D, hiz_texture_);
				cull_shader_->setInt("u_hizTexture", 15);
				cull_shader_->setMat4("u_prevViewProjection", prev_view_projection_);
				cull_shader_->setVec2("u_hizSize", glm::vec2(hiz_width_, hiz_height_));
				cull_shader_->setInt("u_hizMipCount", hiz_mips_);
			}

			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 41, terrain_metadata_ssbo_);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 42, terrain_indirect_args_ssbo_);
			glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, terrain_command_counter_buffer_);

			// Set FrustumData UBO (assumed already bound by Visualizer to binding 3)

			glDispatchCompute((max_chunks_ + 63) / 64, 1, 1);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
		}

		// Update prev view projection for next frame
		// Wait, this needs to be passed from Visualizer correctly.
		// Actually Visualizer passes the CURRENT VP which will be the PREV VP in the next frame's culling.
		// So we should capture the VP that was used for the current depth buffer generation.
	}

	void TerrainRenderManager::UpdateGridTextures(float world_scale) {
		int grid_size = Constants::Class::Terrain::SliceMapSize();
		int half_grid = grid_size / 2;

		float scaled_chunk_size = chunk_size_ * world_scale;
		int   center_chunk_x = static_cast<int>(std::floor(last_camera_pos_.x / scaled_chunk_size));
		int   center_chunk_z = static_cast<int>(std::floor(last_camera_pos_.z / scaled_chunk_size));

		int origin_x = center_chunk_x - half_grid;
		int origin_z = center_chunk_z - half_grid;

		std::vector<int16_t> slice_data(grid_size * grid_size, -1);
		std::vector<float>   height_data(grid_size * grid_size, -10000.0f);

		for (const auto& [key, chunk] : chunks_) {
			int lx = key.first - origin_x;
			int lz = key.second - origin_z;

			if (lx >= 0 && lx < grid_size && lz >= 0 && lz < grid_size) {
				int idx = lz * grid_size + lx;
				slice_data[idx] = static_cast<int16_t>(chunk.texture_slice);
				height_data[idx] = chunk.max_y;
			}
		}

		glBindTexture(GL_TEXTURE_2D, chunk_grid_texture_);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, grid_size, grid_size, GL_RED_INTEGER, GL_SHORT, slice_data.data());

		glBindTexture(GL_TEXTURE_2D, max_height_grid_texture_);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, grid_size, grid_size, GL_RED, GL_FLOAT, height_data.data());

		GenerateMaxHeightMips();

		TerrainDataUbo ubo{};
		ubo.origin_size = glm::ivec4(origin_x, origin_z, grid_size, 1);
		ubo.terrain_params = glm::vec4(static_cast<float>(chunk_size_), world_scale, 0.0f, 0.0f);

		glBindBuffer(GL_UNIFORM_BUFFER, terrain_data_ubo_);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(TerrainDataUbo), &ubo);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	void TerrainRenderManager::GenerateMaxHeightMips() {
		if (!grid_mip_shader_ || !grid_mip_shader_->isValid())
			return;

		int grid_size = Constants::Class::Terrain::SliceMapSize();
		int mips = 1 + static_cast<int>(std::floor(std::log2(grid_size)));

		grid_mip_shader_->use();

		for (int mip = 1; mip < mips; ++mip) {
			int dst_w = std::max(1, grid_size >> mip);
			int dst_h = std::max(1, grid_size >> mip);

			grid_mip_shader_->setInt("u_srcLevel", mip - 1);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, max_height_grid_texture_);
			// Reset levels to allow access to all levels during mip generation
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mips - 1);
			grid_mip_shader_->setInt("u_srcDepth", 0);

			glBindImageTexture(0, max_height_grid_texture_, mip, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

			glDispatchCompute((dst_w + 7) / 8, (dst_h + 7) / 8, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
		}

		glBindTexture(GL_TEXTURE_2D, max_height_grid_texture_);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mips - 1);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void TerrainRenderManager::BindTerrainData(ShaderBase& shader_base) const {
		shader_base.use();
		glActiveTexture(GL_TEXTURE11);
		glBindTexture(GL_TEXTURE_2D, chunk_grid_texture_);
		shader_base.setInt("u_chunkGrid", 11);

		glActiveTexture(GL_TEXTURE12);
		glBindTexture(GL_TEXTURE_2D, max_height_grid_texture_);
		shader_base.setInt("u_maxHeightGrid", 12);

		glActiveTexture(GL_TEXTURE13);
		glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture_);
		shader_base.setInt("u_heightmapArray", 13);

		if (extra_noise_texture_ != 0) {
			glActiveTexture(GL_TEXTURE8);
			glBindTexture(GL_TEXTURE_3D, extra_noise_texture_);
			shader_base.setInt("u_extraNoiseTexture", 8);
		}

		glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::TerrainData(), terrain_data_ubo_);
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

		if (terrain_vao_ == 0 || indices_per_chunk_ == 0) {
			return;
		}

		shader.use();
		shader.setMat4("view", view);
		shader.setMat4("projection", projection);
		shader.setVec2("uViewportSize", viewport_size);
		shader.setMat4("model", glm::mat4(1.0f));
		shader.setFloat("uChunkSize", chunk_size_ * last_world_scale_);

		if (clip_plane) {
			shader.setVec4("clipPlane", *clip_plane);
		} else {
			shader.setVec4("clipPlane", glm::vec4(0, 0, 0, 0));
		}

		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_3D, noise_texture_);
		shader.setInt("u_noiseTexture", 5);

		glActiveTexture(GL_TEXTURE6);
		glBindTexture(GL_TEXTURE_3D, curl_texture_);
		shader.setInt("u_curlTexture", 6);
		if (extra_noise_texture_ != 0) {
			glActiveTexture(GL_TEXTURE8);
			glBindTexture(GL_TEXTURE_3D, extra_noise_texture_);
			shader.setInt("u_extraNoiseTexture", 8);
		}

		// Bind SSBOs for the vertex shader
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 40, terrain_vertices_ssbo_);

		// Bind Biome UBO
		glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::Biomes(), biome_ubo_);

		glBindVertexArray(terrain_vao_);

		// Multi-draw indirect for all visible chunks
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, terrain_indirect_args_ssbo_);
		glBindBuffer(GL_PARAMETER_BUFFER, terrain_command_counter_buffer_);

		glMultiDrawElementsIndirectCount(GL_TRIANGLES, GL_UNSIGNED_INT, (void*)0, 0, max_chunks_, 0);

		glBindVertexArray(0);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
		glBindBuffer(GL_PARAMETER_BUFFER, 0);
	}

	size_t TerrainRenderManager::GetRegisteredChunkCount() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return chunks_.size();
	}

	size_t TerrainRenderManager::GetVisibleChunkCount() const {
		std::lock_guard<std::mutex> lock(mutex_);
		// This is tricky because the count is on GPU.
		// For statistics, we'd need to read back the counter.
		// For now, return an estimate or 0.
		return 0;
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

	std::vector<TerrainRenderManager::DecorChunkData> TerrainRenderManager::GetDecorChunkData(float world_scale) const {
		std::lock_guard<std::mutex> lock(mutex_);
		std::vector<DecorChunkData> result;
		result.reserve(chunks_.size());
		float scaled_chunk_size = static_cast<float>(chunk_size_ * world_scale);
		for (const auto& [key, chunk] : chunks_) {
			result.push_back({
				key,
				chunk.world_offset,
				static_cast<float>(chunk.texture_slice),
				scaled_chunk_size,
				chunk.update_count,
			});
		}
		return result;
	}

} // namespace Boidsish
