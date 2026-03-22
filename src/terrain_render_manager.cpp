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
		cull_shader_ = std::make_unique<ComputeShader>("shaders/terrain_cull.comp");
		bake_shader_ = std::make_unique<ComputeShader>("shaders/terrain_bake.comp");
		mip_gen_shader_ = std::make_unique<ComputeShader>("shaders/terrain_mip_gen.comp");

		if (bake_shader_ && bake_shader_->isValid()) {
			GLuint biomes_idx = glGetUniformBlockIndex(bake_shader_->ID, "BiomeData");
			if (biomes_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(bake_shader_->ID, biomes_idx, Constants::UboBinding::Biomes());
			}
			GLuint lighting_idx = glGetUniformBlockIndex(bake_shader_->ID, "Lighting");
			if (lighting_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(bake_shader_->ID, lighting_idx, Constants::UboBinding::Lighting());
			}
		}

		if (cull_shader_ && cull_shader_->isValid()) {
			cull_num_chunks_loc_ = glGetUniformLocation(cull_shader_->ID, "u_numChunks");
			cull_max_visible_patches_loc_ = glGetUniformLocation(cull_shader_->ID, "u_maxVisiblePatches");
			cull_chunk_size_loc_ = glGetUniformLocation(cull_shader_->ID, "u_chunkSize");
			for (int i = 0; i < 6; ++i) {
				std::string name = "u_frustumPlanes[" + std::to_string(i) + "]";
				cull_frustum_planes_loc_[i] = glGetUniformLocation(cull_shader_->ID, name.c_str());
			}
			cull_camera_pos_loc_ = glGetUniformLocation(cull_shader_->ID, "u_cameraPos");
			cull_chunk_grid_loc_ = glGetUniformLocation(cull_shader_->ID, "u_chunkGrid");
			cull_max_height_grid_loc_ = glGetUniformLocation(cull_shader_->ID, "u_maxHeightGrid");
			cull_heightmap_array_loc_ = glGetUniformLocation(cull_shader_->ID, "u_heightmapArray");
		}

		// GPU Culling Resources
		glGenBuffers(1, &chunk_metadata_ssbo_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunk_metadata_ssbo_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, max_chunks * sizeof(ChunkMetadataGPU), nullptr, GL_DYNAMIC_DRAW);

		glGenBuffers(1, &visible_patches_ssbo_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, visible_patches_ssbo_);
		// Max 64 patches per chunk. Each VisiblePatch is 16 bytes.
		glBufferData(GL_SHADER_STORAGE_BUFFER, max_chunks * 64 * 16, nullptr, GL_DYNAMIC_DRAW);

		glGenBuffers(1, &indirect_buffer_);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirect_buffer_);
		glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(DrawElementsIndirectCommand), nullptr, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

		CreateGridMesh();
		EnsureTextureCapacity(max_chunks);

		// Force initial grid update
		last_grid_origin_x_ = 1000000;
		last_grid_origin_z_ = 1000000;
	}

	TerrainRenderManager::~TerrainRenderManager() {
		if (grid_vao_) glDeleteVertexArrays(1, &grid_vao_);
		if (grid_vbo_) glDeleteBuffers(1, &grid_vbo_);
		if (grid_ebo_) glDeleteBuffers(1, &grid_ebo_);
		if (chunk_metadata_ssbo_) glDeleteBuffers(1, &chunk_metadata_ssbo_);
		if (visible_patches_ssbo_) glDeleteBuffers(1, &visible_patches_ssbo_);
		if (indirect_buffer_) glDeleteBuffers(1, &indirect_buffer_);
		if (heightmap_texture_) glDeleteTextures(1, &heightmap_texture_);
		if (biome_texture_) glDeleteTextures(1, &biome_texture_);
		if (baked_material_texture_) glDeleteTextures(1, &baked_material_texture_);
		if (baked_normal_texture_) glDeleteTextures(1, &baked_normal_texture_);
		if (biome_ubo_) glDeleteBuffers(1, &biome_ubo_);
		if (chunk_grid_texture_) glDeleteTextures(1, &chunk_grid_texture_);
		if (max_height_grid_texture_) glDeleteTextures(1, &max_height_grid_texture_);
		if (terrain_data_ubo_) glDeleteBuffers(1, &terrain_data_ubo_);
	}

	void TerrainRenderManager::CreateGridMesh() {
		std::vector<float> vertices = {
			0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
			1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
			1.0f, 0.0f, 1.0f, 1.0f, 1.0f,
			0.0f, 0.0f, 1.0f, 0.0f, 1.0f
		};
		std::vector<unsigned int> indices = {0, 1, 2, 3};
		grid_index_count_ = indices.size();

		glGenVertexArrays(1, &grid_vao_);
		glBindVertexArray(grid_vao_);
		glGenBuffers(1, &grid_vbo_);
		glBindBuffer(GL_ARRAY_BUFFER, grid_vbo_);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
		glGenBuffers(1, &grid_ebo_);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, grid_ebo_);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
		glBindVertexArray(0);
	}

	void TerrainRenderManager::EnsureTextureCapacity(int required_slices) {
		if (heightmap_texture_ && biome_texture_ && baked_material_texture_ && baked_normal_texture_ && required_slices <= max_chunks_) return;

		GLint max_layers = 0;
		glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &max_layers);
		if (max_layers <= 0) max_layers = 512;

		int new_capacity = std::max(max_chunks_, required_slices);
		if (new_capacity > max_layers) new_capacity = max_layers;

		if (heightmap_texture_ && new_capacity <= max_chunks_) return;

		int old_capacity = max_chunks_;
		int old_slice_count = next_slice_; // slices 0..next_slice_-1 have data
		GLuint old_heightmap = heightmap_texture_;
		GLuint old_biome = biome_texture_;
		GLuint old_baked_material = baked_material_texture_;
		GLuint old_baked_normal = baked_normal_texture_;
		heightmap_texture_ = 0;
		biome_texture_ = 0;
		baked_material_texture_ = 0;
		baked_normal_texture_ = 0;

		max_chunks_ = new_capacity;

		// Resize SSBOs to new capacity
		// Metadata: read-back old data, reallocate, re-upload
		std::vector<ChunkMetadataGPU> old_metadata;
		if (chunk_metadata_ssbo_ && next_gpu_index_ > 0) {
			old_metadata.resize(next_gpu_index_);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunk_metadata_ssbo_);
			glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, next_gpu_index_ * sizeof(ChunkMetadataGPU), old_metadata.data());
		}
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunk_metadata_ssbo_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, max_chunks_ * sizeof(ChunkMetadataGPU), nullptr, GL_DYNAMIC_DRAW);
		if (!old_metadata.empty()) {
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, old_metadata.size() * sizeof(ChunkMetadataGPU), old_metadata.data());
		}

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, visible_patches_ssbo_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, max_chunks_ * 64 * 16, nullptr, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		// Create new larger texture arrays using immutable storage
		glGenTextures(1, &heightmap_texture_);
		glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture_);
		glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA16F, heightmap_resolution_, heightmap_resolution_, max_chunks_);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glGenTextures(1, &biome_texture_);
		glBindTexture(GL_TEXTURE_2D_ARRAY, biome_texture_);
		glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RG8, heightmap_resolution_, heightmap_resolution_, max_chunks_);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		int bake_mips = 1 + static_cast<int>(std::floor(std::log2(kBakeResolution)));

		glGenTextures(1, &baked_material_texture_);
		glBindTexture(GL_TEXTURE_2D_ARRAY, baked_material_texture_);
		glTexStorage3D(GL_TEXTURE_2D_ARRAY, bake_mips, GL_RGBA8, kBakeResolution, kBakeResolution, max_chunks_);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glGenTextures(1, &baked_normal_texture_);
		glBindTexture(GL_TEXTURE_2D_ARRAY, baked_normal_texture_);
		glTexStorage3D(GL_TEXTURE_2D_ARRAY, bake_mips, GL_RGBA8, kBakeResolution, kBakeResolution, max_chunks_);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		// Copy existing slices from old textures to new ones
		if (old_heightmap && old_slice_count > 0) {
			GLuint copy_fbo = 0;
			glGenFramebuffers(1, &copy_fbo);
			for (int s = 0; s < old_slice_count; ++s) {
				// Copy heightmap slice
				glBindFramebuffer(GL_READ_FRAMEBUFFER, copy_fbo);
				glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, old_heightmap, 0, s);
				glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture_);
				glCopyTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, s, 0, 0, heightmap_resolution_, heightmap_resolution_);

				// Copy biome slice
				glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, old_biome, 0, s);
				glBindTexture(GL_TEXTURE_2D_ARRAY, biome_texture_);
				glCopyTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, s, 0, 0, heightmap_resolution_, heightmap_resolution_);

				// Copy baked material slice
				if (old_baked_material) {
					glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, old_baked_material, 0, s);
					glBindTexture(GL_TEXTURE_2D_ARRAY, baked_material_texture_);
					glCopyTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, s, 0, 0, kBakeResolution, kBakeResolution);
				}

				// Copy baked normal slice
				if (old_baked_normal) {
					glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, old_baked_normal, 0, s);
					glBindTexture(GL_TEXTURE_2D_ARRAY, baked_normal_texture_);
					glCopyTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, s, 0, 0, kBakeResolution, kBakeResolution);
				}
			}
			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
			glDeleteFramebuffers(1, &copy_fbo);
		}

		// Restore mipmaps for baked textures
		for (int s = 0; s < old_slice_count; ++s) {
			GenerateBakedMipmaps(s);
		}

		// Clean up old textures
		if (old_heightmap) glDeleteTextures(1, &old_heightmap);
		if (old_biome) glDeleteTextures(1, &old_biome);
		if (old_baked_material) glDeleteTextures(1, &old_baked_material);
		if (old_baked_normal) glDeleteTextures(1, &old_baked_normal);

		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

		// Mark metadata dirty so grid textures get rebuilt
		chunk_metadata_dirty_ = true;
	}

	void TerrainRenderManager::UploadHeightmapSlice(int slice, const std::vector<float>& heightmap, const std::vector<glm::vec3>& normals, const std::vector<glm::vec2>& biomes) {
		const int num_pixels = heightmap_resolution_ * heightmap_resolution_;
		std::vector<float> packed_data;
		packed_data.reserve(num_pixels * 4);
		for (int i = 0; i < num_pixels; ++i) {
			packed_data.push_back(heightmap[i]);
			packed_data.push_back(normals[i].x);
			packed_data.push_back(normals[i].y);
			packed_data.push_back(normals[i].z);
		}
		glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture_);
		glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, slice, heightmap_resolution_, heightmap_resolution_, 1, GL_RGBA, GL_FLOAT, packed_data.data());

		std::vector<uint8_t> biome_data;
		biome_data.reserve(num_pixels * 2);
		for (int i = 0; i < num_pixels; ++i) {
			biome_data.push_back(static_cast<uint8_t>(biomes[i].x));
			biome_data.push_back(static_cast<uint8_t>(biomes[i].y * 255.0f + 0.5f));
		}
		glBindTexture(GL_TEXTURE_2D_ARRAY, biome_texture_);
		glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, slice, heightmap_resolution_, heightmap_resolution_, 1, GL_RG, GL_UNSIGNED_BYTE, biome_data.data());
		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	}

	void TerrainRenderManager::RegisterChunk(std::pair<int, int> chunk_key, const std::vector<glm::vec3>& positions, const std::vector<glm::vec3>& normals, const std::vector<glm::vec2>& biomes, const std::vector<unsigned int>& indices, float min_y, float max_y, const glm::vec3& world_offset, float world_scale) {
		bool should_notify_eviction = false;
		std::pair<int, int> evicted_chunk_key;
		const int res = heightmap_resolution_;
		std::vector<float> heightmap(res * res);
		std::vector<glm::vec3> reordered_normals(res * res);
		std::vector<glm::vec2> reordered_biomes(res * res);
		for (int x = 0; x < res; ++x) {
			for (int z = 0; z < res; ++z) {
				int src_idx = x * res + z;
				int dst_idx = z * res + x;
				heightmap[dst_idx] = positions[src_idx].y;
				reordered_normals[dst_idx] = normals[src_idx];
				reordered_biomes[dst_idx] = biomes[src_idx];
			}
		}
		{
			std::lock_guard<std::recursive_mutex> lock(mutex_);
			auto it = chunks_.find(chunk_key);
			if (it != chunks_.end()) {
				UploadHeightmapSlice(it->second.texture_slice, heightmap, reordered_normals, reordered_biomes);
				it->second.min_y = min_y;
				it->second.max_y = max_y;
				it->second.update_count++;
				chunk_metadata_dirty_ = true;
				return;
			}
			int slice;
			if (!free_slices_.empty()) {
				slice = free_slices_.back();
				free_slices_.pop_back();
			} else if (next_slice_ < max_chunks_) {
				slice = next_slice_++;
			} else {
				// Out of slices — try to grow or evict
				GLint max_layers = 0;
				glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &max_layers);
				if (max_chunks_ < max_layers) {
					// Grow the texture array. This is non-destructive: we just
					// increase capacity. Existing slices and metadata remain valid.
					int new_capacity = std::min(max_chunks_ * 2, max_layers);
					EnsureTextureCapacity(new_capacity);
					slice = next_slice_++;
				} else {
					// At GPU capacity — LRU evict the farthest chunk.
					glm::vec2 camera_pos_2d(last_camera_pos_.x, last_camera_pos_.z);
					float max_dist_sq = -1.0f;
					std::pair<int, int> farthest_key;
					for (const auto& [key, chunk] : chunks_) {
						float sc = chunk_size_ * last_world_scale_;
						glm::vec2 center(chunk.world_offset.x + sc * 0.5f, chunk.world_offset.y + sc * 0.5f);
						float dist_sq = glm::dot(center - camera_pos_2d, center - camera_pos_2d);
						if (dist_sq > max_dist_sq) {
							max_dist_sq = dist_sq;
							farthest_key = key;
						}
					}
					if (max_dist_sq < 0) return;

					// Capture what we need BEFORE unregistering, then manually
					// reclaim the slice and gpu_index without going through
					// UnregisterChunk (which would push them to free pools and
					// cause double-allocation).
					auto evict_it = chunks_.find(farthest_key);
					slice = evict_it->second.texture_slice;
					int evicted_gpu_index = evict_it->second.gpu_index;

					// Deactivate in GPU metadata immediately
					ChunkMetadataGPU inactive{};
					inactive.world_offset_slice.w = 0.0f;
					glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunk_metadata_ssbo_);
					glBufferSubData(GL_SHADER_STORAGE_BUFFER, evicted_gpu_index * sizeof(ChunkMetadataGPU), sizeof(ChunkMetadataGPU), &inactive);

					// Remove from map but DON'T push slice/gpu_index to free pools —
					// we're reusing them directly for the new chunk.
					chunks_.erase(evict_it);
					evicted_chunk_key = farthest_key;
					should_notify_eviction = true;

					// Reuse the evicted gpu_index directly
					free_gpu_indices_.push_back(evicted_gpu_index);
				}
			}
			UploadHeightmapSlice(slice, heightmap, reordered_normals, reordered_biomes);
			int gpu_index;
			if (!free_gpu_indices_.empty()) {
				gpu_index = free_gpu_indices_.back();
				free_gpu_indices_.pop_back();
			} else gpu_index = next_gpu_index_++;
			ChunkInfo info{};
			info.texture_slice = slice;
			info.min_y = min_y;
			info.max_y = max_y;
			info.world_offset = glm::vec2(world_offset.x, world_offset.z);
			info.gpu_index = gpu_index;
			chunks_[chunk_key] = info;
			chunk_metadata_dirty_ = true;

			// Bake the chunk appearance immediately after upload
			if (bake_shader_ && bake_shader_->isValid()) {
				bake_shader_->use();
				bake_shader_->setFloat("u_chunkSize", static_cast<float>(chunk_size_ * world_scale));
				bake_shader_->setFloat("u_worldScale", world_scale);
				bake_shader_->setInt("u_textureSlice", slice);
				bake_shader_->setVec2("u_worldOffset", glm::vec2(world_offset.x, world_offset.z));

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture_);
				bake_shader_->setInt("u_heightmapArray", 0);

				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D_ARRAY, biome_texture_);
				bake_shader_->setInt("uBiomeMap", 1);

				glActiveTexture(GL_TEXTURE5);
				glBindTexture(GL_TEXTURE_3D, noise_texture_);
				bake_shader_->setInt("u_noiseTexture", 5);

				glActiveTexture(GL_TEXTURE6);
				glBindTexture(GL_TEXTURE_3D, curl_texture_);
				bake_shader_->setInt("u_curlTexture", 6);

				glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::Biomes(), biome_ubo_);
				if (lighting_ubo_ != 0) {
					glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::Lighting(), lighting_ubo_);
				}

				glBindImageTexture(0, baked_material_texture_, 0, GL_FALSE, slice, GL_WRITE_ONLY, GL_RGBA8);
				glBindImageTexture(1, baked_normal_texture_, 0, GL_FALSE, slice, GL_WRITE_ONLY, GL_RGBA8);

				glDispatchCompute((kBakeResolution + 7) / 8, (kBakeResolution + 7) / 8, 1);
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

				GenerateBakedMipmaps(slice);
			}
		}
		if (should_notify_eviction && eviction_callback_) eviction_callback_(evicted_chunk_key);
	}

	void TerrainRenderManager::UnregisterChunk(std::pair<int, int> chunk_key) {
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		auto it = chunks_.find(chunk_key);
		if (it == chunks_.end()) return;
		free_slices_.push_back(it->second.texture_slice);
		ChunkMetadataGPU inactive{};
		inactive.world_offset_slice.w = 0.0f;
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunk_metadata_ssbo_);
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, it->second.gpu_index * sizeof(ChunkMetadataGPU), sizeof(ChunkMetadataGPU), &inactive);
		free_gpu_indices_.push_back(it->second.gpu_index);
		chunks_.erase(it);
		chunk_metadata_dirty_ = true;
	}

	bool TerrainRenderManager::HasChunk(std::pair<int, int> chunk_key) const {
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		return chunks_.count(chunk_key) > 0;
	}

	bool TerrainRenderManager::IsChunkVisible(const ChunkInfo& chunk, const Frustum& frustum, float world_scale) const {
		float scaled_chunk_size = chunk_size_ * world_scale;
		glm::vec3 min_corner(chunk.world_offset.x, chunk.min_y, chunk.world_offset.y);
		glm::vec3 max_corner(chunk.world_offset.x + scaled_chunk_size, chunk.max_y, chunk.world_offset.y + scaled_chunk_size);
		glm::vec3 center = (min_corner + max_corner) * 0.5f;
		glm::vec3 half_size = (max_corner - min_corner) * 0.5f;
		for (int i = 0; i < 6; ++i) {
			float r = half_size.x * std::abs(frustum.planes[i].normal.x) + half_size.y * std::abs(frustum.planes[i].normal.y) + half_size.z * std::abs(frustum.planes[i].normal.z);
			float d = glm::dot(center, frustum.planes[i].normal) + frustum.planes[i].distance;
			if (d < -r) return false;
		}
		return true;
	}

	void TerrainRenderManager::UpdateChunkMetadata() {
		if (!chunk_metadata_dirty_) return;
		std::vector<ChunkMetadataGPU> gpu_data(next_gpu_index_);
		for (auto& d : gpu_data) d.world_offset_slice.w = 0.0f;
		for (const auto& [key, chunk] : chunks_) {
			if (chunk.gpu_index >= 0 && chunk.gpu_index < (int)gpu_data.size()) {
				auto& d = gpu_data[chunk.gpu_index];
				d.world_offset_slice = glm::vec4(chunk.world_offset.x, chunk.world_offset.y, static_cast<float>(chunk.texture_slice), 1.0f);
				d.bounds = glm::vec4(chunk.min_y, chunk.max_y, 0.0f, 0.0f);
			}
		}
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunk_metadata_ssbo_);
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, gpu_data.size() * sizeof(ChunkMetadataGPU), gpu_data.data());
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		chunk_metadata_dirty_ = false;
	}

	void TerrainRenderManager::PrepareForRender(const Frustum& frustum, const glm::vec3& camera_pos, float world_scale) {
		std::lock_guard<std::recursive_mutex> lock(mutex_);

		last_camera_pos_ = camera_pos;
		last_world_scale_ = world_scale;
		UpdateGridTextures(world_scale);
		UpdateChunkMetadata();
		if (chunks_.empty() || !cull_shader_ || !cull_shader_->isValid()) return;

		int max_visible_patches = max_chunks_ * 64;
		DrawElementsIndirectCommand cmd{};
		cmd.count = static_cast<uint32_t>(grid_index_count_);
		cmd.instanceCount = 0;
		cmd.firstIndex = 0;
		cmd.baseVertex = 0;
		cmd.baseInstance = 0;
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirect_buffer_);
		glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, sizeof(DrawElementsIndirectCommand), &cmd);
		cull_shader_->use();
		if (cull_num_chunks_loc_ != -1) glUniform1i(cull_num_chunks_loc_, next_gpu_index_);
		if (cull_max_visible_patches_loc_ != -1) glUniform1i(cull_max_visible_patches_loc_, max_visible_patches);
		if (cull_chunk_size_loc_ != -1) glUniform1f(cull_chunk_size_loc_, static_cast<float>(chunk_size_ * world_scale));
		for (int i = 0; i < 6; ++i) {
			if (cull_frustum_planes_loc_[i] != -1) {
				glUniform4f(cull_frustum_planes_loc_[i], frustum.planes[i].normal.x, frustum.planes[i].normal.y, frustum.planes[i].normal.z, frustum.planes[i].distance);
			}
		}

		if (cull_camera_pos_loc_ != -1) glUniform3f(cull_camera_pos_loc_, camera_pos.x, camera_pos.y, camera_pos.z);
		glActiveTexture(GL_TEXTURE11);
		glBindTexture(GL_TEXTURE_2D, chunk_grid_texture_);
		if (cull_chunk_grid_loc_ != -1) glUniform1i(cull_chunk_grid_loc_, 11);
		glActiveTexture(GL_TEXTURE12);
		glBindTexture(GL_TEXTURE_2D, max_height_grid_texture_);
		if (cull_max_height_grid_loc_ != -1) glUniform1i(cull_max_height_grid_loc_, 12);
		glActiveTexture(GL_TEXTURE13);
		glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture_);
		if (cull_heightmap_array_loc_ != -1) glUniform1i(cull_heightmap_array_loc_, 13);

		glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::TerrainData(), terrain_data_ubo_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, chunk_metadata_ssbo_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::TerrainVisiblePatches(), visible_patches_ssbo_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::TerrainIndirect(), indirect_buffer_);

		glDispatchCompute((next_gpu_index_ * 64 + 63) / 64, 1, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
	}

	void TerrainRenderManager::UpdateGridTextures(float world_scale) {
		int grid_size = Constants::Class::Terrain::SliceMapSize();
		int half_grid = grid_size / 2;
		float scaled_chunk_size = chunk_size_ * world_scale;
		int center_chunk_x = static_cast<int>(std::floor(last_camera_pos_.x / scaled_chunk_size));
		int center_chunk_z = static_cast<int>(std::floor(last_camera_pos_.z / scaled_chunk_size));
		int origin_x = center_chunk_x - half_grid;
		int origin_z = center_chunk_z - half_grid;

		bool grid_moved = (origin_x != last_grid_origin_x_ || origin_z != last_grid_origin_z_);
		bool scale_changed = (world_scale != last_grid_world_scale_);

		if (grid_moved || scale_changed || chunk_metadata_dirty_) {
			std::vector<int16_t> slice_data(grid_size * grid_size, -1);
			std::vector<float> height_data(grid_size * grid_size, -10000.0f);
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

			last_grid_origin_x_ = origin_x;
			last_grid_origin_z_ = origin_z;
			last_grid_world_scale_ = world_scale;
		}
	}

	void TerrainRenderManager::GenerateBakedMipmaps(int slice) {
		if (!mip_gen_shader_ || !mip_gen_shader_->isValid()) return;

		int mips = 1 + static_cast<int>(std::floor(std::log2(kBakeResolution)));
		mip_gen_shader_->use();
		mip_gen_shader_->setInt("u_slice", slice);

		auto generate_for_tex = [&](GLuint texture) {
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
			mip_gen_shader_->setInt("u_srcTexture", 0);

			for (int mip = 1; mip < mips; ++mip) {
				int dst_size = std::max(1, kBakeResolution >> mip);
				mip_gen_shader_->setInt("u_srcLevel", mip - 1);

				glBindImageTexture(0, texture, mip, GL_FALSE, slice, GL_WRITE_ONLY, GL_RGBA8);
				glDispatchCompute((dst_size + 7) / 8, (dst_size + 7) / 8, 1);
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
			}
		};

		generate_for_tex(baked_material_texture_);
		generate_for_tex(baked_normal_texture_);
		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	}

	void TerrainRenderManager::GenerateMaxHeightMips() {
		if (!grid_mip_shader_ || !grid_mip_shader_->isValid()) return;
		int grid_size = Constants::Class::Terrain::SliceMapSize();
		int mips = 1 + static_cast<int>(std::floor(std::log2(grid_size)));
		grid_mip_shader_->use();
		for (int mip = 1; mip < mips; ++mip) {
			int dst_w = std::max(1, grid_size >> mip);
			int dst_h = std::max(1, grid_size >> mip);
			grid_mip_shader_->setInt("u_srcLevel", mip - 1);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, max_height_grid_texture_);
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
		glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::TerrainData(), terrain_data_ubo_);
	}

	void TerrainRenderManager::Render(Shader& shader, const glm::mat4& view, const glm::mat4& projection, const glm::vec2& viewport_size, const std::optional<glm::vec4>& clip_plane, float tess_quality_multiplier) {
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		if (chunks_.empty() || grid_vao_ == 0 || grid_index_count_ == 0) return;
		shader.use();
		shader.setMat4("view", view);
		shader.setMat4("projection", projection);
		shader.setVec2("uViewportSize", viewport_size);
		shader.setMat4("model", glm::mat4(1.0f));
		shader.setFloat("uTessQualityMultiplier", tess_quality_multiplier);
		shader.setFloat("uTessLevelMax", 64.0f);
		shader.setFloat("uTessLevelMin", 1.0f);
		shader.setFloat("uChunkSize", chunk_size_ * last_world_scale_);
		if (clip_plane) shader.setVec4("clipPlane", *clip_plane);
		else shader.setVec4("clipPlane", glm::vec4(0, 0, 0, 0));
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture_);
		shader.setInt("uHeightmap", 0);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D_ARRAY, biome_texture_);
		shader.setInt("uBiomeMap", 1);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D_ARRAY, baked_material_texture_);
		shader.setInt("uBakedMaterial", 2);
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D_ARRAY, baked_normal_texture_);
		shader.setInt("uBakedNormal", 3);
		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_3D, noise_texture_);
		shader.setInt("u_noiseTexture", 5);
		glActiveTexture(GL_TEXTURE6);
		glBindTexture(GL_TEXTURE_3D, curl_texture_);
		shader.setInt("u_curlTexture", 6);
		glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::Biomes(), biome_ubo_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, chunk_metadata_ssbo_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::TerrainVisiblePatches(), visible_patches_ssbo_);
		glBindVertexArray(grid_vao_);
		glPatchParameteri(GL_PATCH_VERTICES, 4);

		// Clamp instanceCount to buffer capacity.
		// The cull shader guards writes but the atomic counter can still exceed capacity.
		int max_patches = max_chunks_ * 64;
		GLuint clamped_count = static_cast<GLuint>(max_patches);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirect_buffer_);
		glGetBufferSubData(GL_DRAW_INDIRECT_BUFFER, offsetof(DrawElementsIndirectCommand, instanceCount), sizeof(GLuint), &clamped_count);
		if (clamped_count > static_cast<GLuint>(max_patches)) {
			clamped_count = static_cast<GLuint>(max_patches);
			glBufferSubData(GL_DRAW_INDIRECT_BUFFER, offsetof(DrawElementsIndirectCommand, instanceCount), sizeof(GLuint), &clamped_count);
		}
		cached_visible_patch_count_ = clamped_count;
		glDrawElementsIndirect(GL_PATCHES, GL_UNSIGNED_INT, nullptr);
		glBindVertexArray(0);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	}

	size_t TerrainRenderManager::GetRegisteredChunkCount() const {
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		return chunks_.size();
	}

	size_t TerrainRenderManager::GetVisibleChunkCount() const {
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		return cached_visible_patch_count_;
	}

	std::vector<glm::vec4> TerrainRenderManager::GetChunkInfo(float world_scale) const {
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		std::vector<glm::vec4>      result;
		result.reserve(chunks_.size());
		for (const auto& [key, chunk] : chunks_) {
			result.push_back(glm::vec4(chunk.world_offset.x, chunk.world_offset.y, static_cast<float>(chunk.texture_slice), static_cast<float>(chunk_size_ * world_scale)));
		}
		return result;
	}

	std::vector<TerrainRenderManager::DecorChunkData> TerrainRenderManager::GetDecorChunkData(float world_scale) const {
		std::lock_guard<std::recursive_mutex>     lock(mutex_);
		std::vector<DecorChunkData>     result;
		result.reserve(chunks_.size());
		float scaled_chunk_size = static_cast<float>(chunk_size_ * world_scale);
		for (const auto& [key, chunk] : chunks_) {
			result.push_back({key, chunk.world_offset, static_cast<float>(chunk.texture_slice), scaled_chunk_size, chunk.update_count});
		}
		return result;
	}
}
