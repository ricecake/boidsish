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
		chunk_size_(chunk_size),
		max_chunks_(max_chunks),
		vertices_per_chunk_((chunk_size + 1) * (chunk_size + 1)),
		heightmap_resolution_(chunk_size + 1) {
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

		glGenBuffers(1, &indirect_buffer_);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirect_buffer_);
		glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(DrawElementsIndirectCommand), nullptr, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

		CreateMeshBuffers();
		CreatePatchIndexTemplate();
		EnsureBufferCapacity(max_chunks);
		CreateMeshBuffers();

		// Force initial grid update
		last_grid_origin_x_ = 1000000;
		last_grid_origin_z_ = 1000000;
	}

	TerrainRenderManager::~TerrainRenderManager() {
		if (mesh_vao_)
			glDeleteVertexArrays(1, &mesh_vao_);
		if (terrain_vbo_)
			glDeleteBuffers(1, &terrain_vbo_);
		if (dynamic_ebo_)
			glDeleteBuffers(1, &dynamic_ebo_);
		if (patch_indices_ssbo_)
			glDeleteBuffers(1, &patch_indices_ssbo_);
		if (chunk_metadata_ssbo_)
			glDeleteBuffers(1, &chunk_metadata_ssbo_);
		if (indirect_buffer_)
			glDeleteBuffers(1, &indirect_buffer_);
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

	void TerrainRenderManager::CreateMeshBuffers() {
		if (mesh_vao_ == 0) {
			glGenVertexArrays(1, &mesh_vao_);
		}
		glBindVertexArray(mesh_vao_);

		glBindBuffer(GL_ARRAY_BUFFER, terrain_vbo_);

		// Position: vec3
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, position));

		// Normal: vec3
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, normal));

		// Biome: vec2
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, biome));

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dynamic_ebo_);

		glBindVertexArray(0);
	}

	void TerrainRenderManager::CreatePatchIndexTemplate() {
		// Create a template for a single 4x4 quad patch (5x5 vertices)
		// Indices are relative to the patch origin within the chunk grid
		int       chunk_vertices_edge = chunk_size_ + 1;
		int       indices_per_patch = (chunk_size_ / 8) * (chunk_size_ / 8) * 6;
		std::vector<unsigned int> template_indices;
		template_indices.reserve(indices_per_patch);

		for (int y = 0; y < chunk_size_ / 8; ++y) {
			for (int x = 0; x < chunk_size_ / 8; ++x) {
				int i00 = y * chunk_vertices_edge + x;
				int i10 = y * chunk_vertices_edge + (x + 1);
				int i01 = (y + 1) * chunk_vertices_edge + x;
				int i11 = (y + 1) * chunk_vertices_edge + (x + 1);

				template_indices.push_back(i00);
				template_indices.push_back(i01);
				template_indices.push_back(i10);
				template_indices.push_back(i10);
				template_indices.push_back(i01);
				template_indices.push_back(i11);
			}
		}

		glGenBuffers(1, &patch_indices_ssbo_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, patch_indices_ssbo_);
		glBufferData(
			GL_SHADER_STORAGE_BUFFER,
			template_indices.size() * sizeof(unsigned int),
			template_indices.data(),
			GL_STATIC_DRAW
		);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}

	void TerrainRenderManager::EnsureBufferCapacity(int required_chunks) {
		if (terrain_vbo_ && dynamic_ebo_ && heightmap_texture_ && required_chunks <= max_chunks_)
			return;

		GLint max_layers = 0;
		glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &max_layers);
		if (max_layers <= 0)
			max_layers = 512;

		int new_capacity = std::max(max_chunks_, required_chunks);
		if (new_capacity > max_layers)
			new_capacity = max_layers;

		if (terrain_vbo_ && new_capacity <= max_chunks_)
			return;

		GLuint old_heightmap = heightmap_texture_;
		GLuint old_biome = biome_texture_;
		GLuint old_vbo = terrain_vbo_;
		GLuint old_ebo = dynamic_ebo_;
		int    old_slice_count = next_base_vertex_ / vertices_per_chunk_;

		max_chunks_ = new_capacity;

		// Resize SSBOs to new capacity
		// Metadata: read-back old data, reallocate, re-upload
		std::vector<ChunkMetadataGPU> old_metadata;
		if (chunk_metadata_ssbo_ && next_gpu_index_ > 0) {
			old_metadata.resize(next_gpu_index_);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunk_metadata_ssbo_);
			glGetBufferSubData(
				GL_SHADER_STORAGE_BUFFER,
				0,
				next_gpu_index_ * sizeof(ChunkMetadataGPU),
				old_metadata.data()
			);
		}
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunk_metadata_ssbo_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, max_chunks_ * sizeof(ChunkMetadataGPU), nullptr, GL_DYNAMIC_DRAW);
		if (!old_metadata.empty()) {
			glBufferSubData(
				GL_SHADER_STORAGE_BUFFER,
				0,
				old_metadata.size() * sizeof(ChunkMetadataGPU),
				old_metadata.data()
			);
		}

		// Large VBO for all chunks
		GLuint new_vbo;
		glGenBuffers(1, &new_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, new_vbo);
		glBufferData(GL_ARRAY_BUFFER, max_chunks_ * vertices_per_chunk_ * sizeof(TerrainVertex), nullptr, GL_STATIC_DRAW);
		if (old_vbo && old_slice_count > 0) {
			glBindBuffer(GL_COPY_READ_BUFFER, old_vbo);
			glCopyBufferSubData(
				GL_COPY_READ_BUFFER,
				GL_ARRAY_BUFFER,
				0,
				0,
				old_slice_count * vertices_per_chunk_ * sizeof(TerrainVertex)
			);
			glDeleteBuffers(1, &old_vbo);
		}
		terrain_vbo_ = new_vbo;

		// Large dynamic EBO for indices of visible patches
		int indices_per_patch = (chunk_size_ / 8) * (chunk_size_ / 8) * 6;
		GLuint new_ebo;
		glGenBuffers(1, &new_ebo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, new_ebo);
		glBufferData(
			GL_ELEMENT_ARRAY_BUFFER,
			max_chunks_ * 64 * indices_per_patch * sizeof(unsigned int),
			nullptr,
			GL_DYNAMIC_DRAW
		);
		if (old_ebo)
			glDeleteBuffers(1, &old_ebo);
		dynamic_ebo_ = new_ebo;

		// Texture arrays
		GLuint new_heightmap;
		glGenTextures(1, &new_heightmap);
		glBindTexture(GL_TEXTURE_2D_ARRAY, new_heightmap);
		glTexImage3D(
			GL_TEXTURE_2D_ARRAY,
			0,
			GL_RGBA16F,
			heightmap_resolution_,
			heightmap_resolution_,
			max_chunks_,
			0,
			GL_RGBA,
			GL_FLOAT,
			nullptr
		);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		GLuint new_biome_tex;
		glGenTextures(1, &new_biome_tex);
		glBindTexture(GL_TEXTURE_2D_ARRAY, new_biome_tex);
		glTexImage3D(
			GL_TEXTURE_2D_ARRAY,
			0,
			GL_RG8,
			heightmap_resolution_,
			heightmap_resolution_,
			max_chunks_,
			0,
			GL_RG,
			GL_UNSIGNED_BYTE,
			nullptr
		);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		// Copy existing texture slices
		if (old_heightmap && old_slice_count > 0) {
			GLuint copy_fbo = 0;
			glGenFramebuffers(1, &copy_fbo);
			for (int s = 0; s < old_slice_count; ++s) {
				glBindFramebuffer(GL_READ_FRAMEBUFFER, copy_fbo);
				glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, old_heightmap, 0, s);
				glBindTexture(GL_TEXTURE_2D_ARRAY, new_heightmap);
				glCopyTexSubImage3D(
					GL_TEXTURE_2D_ARRAY,
					0,
					0,
					0,
					s,
					0,
					0,
					heightmap_resolution_,
					heightmap_resolution_
				);

				glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, old_biome, 0, s);
				glBindTexture(GL_TEXTURE_2D_ARRAY, new_biome_tex);
				glCopyTexSubImage3D(
					GL_TEXTURE_2D_ARRAY,
					0,
					0,
					0,
					s,
					0,
					0,
					heightmap_resolution_,
					heightmap_resolution_
				);
			}
			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
			glDeleteFramebuffers(1, &copy_fbo);
		}

		if (old_heightmap)
			glDeleteTextures(1, &old_heightmap);
		if (old_biome)
			glDeleteTextures(1, &old_biome);

		heightmap_texture_ = new_heightmap;
		biome_texture_ = new_biome_tex;

		if (mesh_vao_ != 0) {
			CreateMeshBuffers();
		}

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

		chunk_metadata_dirty_ = true;
	}

	void TerrainRenderManager::UploadChunkMesh(
		int                           base_vertex,
		const std::vector<glm::vec3>& positions,
		const std::vector<glm::vec3>& normals,
		const std::vector<glm::vec2>& biomes
	) {
		// Find world offset for this base_vertex to upload absolute positions
		glm::vec3 world_offset(0.0f);
		for (const auto& [key, chunk] : chunks_) {
			if (chunk.base_vertex == base_vertex) {
				world_offset = glm::vec3(chunk.world_offset.x, 0.0f, chunk.world_offset.y);
				break;
			}
		}

		std::vector<TerrainVertex> packed_data(vertices_per_chunk_);
		for (int i = 0; i < vertices_per_chunk_; ++i) {
			packed_data[i].position = positions[i] + world_offset;
			packed_data[i].normal = normals[i];
			packed_data[i].biome = biomes[i];
		}

		glBindBuffer(GL_ARRAY_BUFFER, terrain_vbo_);
		glBufferSubData(
			GL_ARRAY_BUFFER,
			base_vertex * sizeof(TerrainVertex),
			vertices_per_chunk_ * sizeof(TerrainVertex),
			packed_data.data()
		);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		// Also upload to heightmap textures for compatibility
		const int              res = heightmap_resolution_;
		std::vector<float>     heightmap(res * res);
		std::vector<glm::vec3> reordered_normals(res * res);
		std::vector<glm::vec2> reordered_biomes(res * res);
		for (int x = 0; x < res; ++x) {
			for (int z = 0; z < res; ++z) {
				int src_idx = x * res + z;
				int dst_idx = z * res + x;
				heightmap[dst_idx] = positions[src_idx].y + world_offset.y;
				reordered_normals[dst_idx] = normals[src_idx];
				reordered_biomes[dst_idx] = biomes[src_idx];
			}
		}
		UploadHeightmapSlice(base_vertex / vertices_per_chunk_, heightmap, reordered_normals, reordered_biomes);
	}

	void TerrainRenderManager::UploadHeightmapSlice(
		int                           slice,
		const std::vector<float>&     heightmap,
		const std::vector<glm::vec3>& normals,
		const std::vector<glm::vec2>& biomes
	) {
		const int          num_pixels = heightmap_resolution_ * heightmap_resolution_;
		std::vector<float> packed_data;
		packed_data.reserve(num_pixels * 4);
		for (int i = 0; i < num_pixels; ++i) {
			packed_data.push_back(heightmap[i]);
			packed_data.push_back(normals[i].x);
			packed_data.push_back(normals[i].y);
			packed_data.push_back(normals[i].z);
		}
		glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture_);
		glTexSubImage3D(
			GL_TEXTURE_2D_ARRAY,
			0,
			0,
			0,
			slice,
			heightmap_resolution_,
			heightmap_resolution_,
			1,
			GL_RGBA,
			GL_FLOAT,
			packed_data.data()
		);

		std::vector<uint8_t> biome_data;
		biome_data.reserve(num_pixels * 2);
		for (int i = 0; i < num_pixels; ++i) {
			biome_data.push_back(static_cast<uint8_t>(biomes[i].x));
			biome_data.push_back(static_cast<uint8_t>(biomes[i].y * 255.0f + 0.5f));
		}
		glBindTexture(GL_TEXTURE_2D_ARRAY, biome_texture_);
		glTexSubImage3D(
			GL_TEXTURE_2D_ARRAY,
			0,
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
		const std::vector<unsigned int>& indices,
		float                            min_y,
		float                            max_y,
		const glm::vec3&                 world_offset
	) {
		bool                should_notify_eviction = false;
		std::pair<int, int> evicted_chunk_key;
		{
			std::lock_guard<std::recursive_mutex> lock(mutex_);
			auto                                  it = chunks_.find(chunk_key);
			if (it != chunks_.end()) {
				it->second.min_y = min_y;
				it->second.max_y = max_y;
				it->second.update_count++;
				chunk_metadata_dirty_ = true;
				UploadChunkMesh(it->second.base_vertex, positions, normals, biomes);
				return;
			}
			int base_vertex;
			if (!free_base_vertices_.empty()) {
				base_vertex = free_base_vertices_.back();
				free_base_vertices_.pop_back();
			} else if (next_base_vertex_ < max_chunks_ * vertices_per_chunk_) {
				base_vertex = next_base_vertex_;
				next_base_vertex_ += vertices_per_chunk_;
			} else {
				// At GPU capacity — LRU evict the farthest chunk.
				glm::vec2           camera_pos_2d(last_camera_pos_.x, last_camera_pos_.z);
				float               max_dist_sq = -1.0f;
				std::pair<int, int> farthest_key;
				for (const auto& [key, chunk] : chunks_) {
					float     sc = chunk_size_ * last_world_scale_;
					glm::vec2 center(chunk.world_offset.x + sc * 0.5f, chunk.world_offset.y + sc * 0.5f);
					float     dist_sq = glm::dot(center - camera_pos_2d, center - camera_pos_2d);
					if (dist_sq > max_dist_sq) {
						max_dist_sq = dist_sq;
						farthest_key = key;
					}
				}
				if (max_dist_sq < 0)
					return;

				auto evict_it = chunks_.find(farthest_key);
				base_vertex = evict_it->second.base_vertex;
				int evicted_gpu_index = evict_it->second.gpu_index;

				// Deactivate in GPU metadata immediately
				ChunkMetadataGPU inactive{};
				inactive.world_offset_base_vertex.w = 0.0f;
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunk_metadata_ssbo_);
				glBufferSubData(
					GL_SHADER_STORAGE_BUFFER,
					evicted_gpu_index * sizeof(ChunkMetadataGPU),
					sizeof(ChunkMetadataGPU),
					&inactive
				);

				chunks_.erase(evict_it);
				evicted_chunk_key = farthest_key;
				should_notify_eviction = true;

				free_gpu_indices_.push_back(evicted_gpu_index);
			}
			UploadChunkMesh(base_vertex, positions, normals, biomes);
			int gpu_index;
			if (!free_gpu_indices_.empty()) {
				gpu_index = free_gpu_indices_.back();
				free_gpu_indices_.pop_back();
			} else
				gpu_index = next_gpu_index_++;

			ChunkInfo info{};
			info.base_vertex = base_vertex;
			info.min_y = min_y;
			info.max_y = max_y;
			info.world_offset = glm::vec2(world_offset.x, world_offset.z);
			info.gpu_index = gpu_index;
			chunks_[chunk_key] = info;
			chunk_metadata_dirty_ = true;
		}
		if (should_notify_eviction && eviction_callback_)
			eviction_callback_(evicted_chunk_key);
	}

	void TerrainRenderManager::UnregisterChunk(std::pair<int, int> chunk_key) {
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		auto                                  it = chunks_.find(chunk_key);
		if (it == chunks_.end())
			return;
		free_base_vertices_.push_back(it->second.base_vertex);
		ChunkMetadataGPU inactive{};
		inactive.world_offset_base_vertex.w = 0.0f;
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunk_metadata_ssbo_);
		glBufferSubData(
			GL_SHADER_STORAGE_BUFFER,
			it->second.gpu_index * sizeof(ChunkMetadataGPU),
			sizeof(ChunkMetadataGPU),
			&inactive
		);
		free_gpu_indices_.push_back(it->second.gpu_index);
		chunks_.erase(it);
		chunk_metadata_dirty_ = true;
	}

	bool TerrainRenderManager::HasChunk(std::pair<int, int> chunk_key) const {
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		return chunks_.count(chunk_key) > 0;
	}

	bool TerrainRenderManager::IsChunkVisible(const ChunkInfo& chunk, const Frustum& frustum, float world_scale) const {
		float     scaled_chunk_size = chunk_size_ * world_scale;
		glm::vec3 min_corner(chunk.world_offset.x, chunk.min_y, chunk.world_offset.y);
		glm::vec3 max_corner(
			chunk.world_offset.x + scaled_chunk_size,
			chunk.max_y,
			chunk.world_offset.y + scaled_chunk_size
		);
		glm::vec3 center = (min_corner + max_corner) * 0.5f;
		glm::vec3 half_size = (max_corner - min_corner) * 0.5f;
		for (int i = 0; i < 6; ++i) {
			float r = half_size.x * std::abs(frustum.planes[i].normal.x) +
				half_size.y * std::abs(frustum.planes[i].normal.y) + half_size.z * std::abs(frustum.planes[i].normal.z);
			float d = glm::dot(center, frustum.planes[i].normal) + frustum.planes[i].distance;
			if (d < -r)
				return false;
		}
		return true;
	}

	void TerrainRenderManager::UpdateChunkMetadata() {
		if (!chunk_metadata_dirty_)
			return;
		std::vector<ChunkMetadataGPU> gpu_data(next_gpu_index_);
		for (auto& d : gpu_data)
			d.world_offset_base_vertex.w = 0.0f;
		for (const auto& [key, chunk] : chunks_) {
			if (chunk.gpu_index >= 0 && chunk.gpu_index < (int)gpu_data.size()) {
				auto& d = gpu_data[chunk.gpu_index];
				d.world_offset_base_vertex = glm::vec4(
					chunk.world_offset.x,
					chunk.world_offset.y,
					static_cast<float>(chunk.base_vertex),
					1.0f
				);
				d.bounds = glm::vec4(chunk.min_y, chunk.max_y, 0.0f, 0.0f);
			}
		}
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunk_metadata_ssbo_);
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, gpu_data.size() * sizeof(ChunkMetadataGPU), gpu_data.data());
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		chunk_metadata_dirty_ = false;
	}

	void
	TerrainRenderManager::PrepareForRender(const Frustum& frustum, const glm::vec3& camera_pos, float world_scale) {
		std::lock_guard<std::recursive_mutex> lock(mutex_);

		last_camera_pos_ = camera_pos;
		last_world_scale_ = world_scale;
		UpdateGridTextures(world_scale);
		UpdateChunkMetadata();

		// Async readback of visible count from previous frame
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirect_buffer_);
		uint32_t* ptr = (uint32_t*)glMapBufferRange(GL_DRAW_INDIRECT_BUFFER, 0, sizeof(uint32_t), GL_MAP_READ_BIT);
		if (ptr) {
			int indices_per_patch = (chunk_size_ / 8) * (chunk_size_ / 8) * 6;
			cached_visible_patch_count_ = (*ptr) / indices_per_patch;
			glUnmapBuffer(GL_DRAW_INDIRECT_BUFFER);
		}

		if (chunks_.empty() || !cull_shader_ || !cull_shader_->isValid())
			return;

		int                         max_visible_patches = max_chunks_ * 64;
		DrawElementsIndirectCommand cmd{};
		cmd.count = 0; // The cull shader will increment this
		cmd.instanceCount = 1;
		cmd.firstIndex = 0;
		cmd.baseVertex = 0;
		cmd.baseInstance = 0;
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirect_buffer_);
		glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, sizeof(DrawElementsIndirectCommand), &cmd);
		cull_shader_->use();
		if (cull_num_chunks_loc_ != -1)
			glUniform1i(cull_num_chunks_loc_, next_gpu_index_);
		if (cull_max_visible_patches_loc_ != -1)
			glUniform1i(cull_max_visible_patches_loc_, max_visible_patches);
		for (int i = 0; i < 6; ++i) {
			if (cull_frustum_planes_loc_[i] != -1) {
				glUniform4f(
					cull_frustum_planes_loc_[i],
					frustum.planes[i].normal.x,
					frustum.planes[i].normal.y,
					frustum.planes[i].normal.z,
					frustum.planes[i].distance
				);
			}
		}

		if (cull_camera_pos_loc_ != -1)
			glUniform3f(cull_camera_pos_loc_, camera_pos.x, camera_pos.y, camera_pos.z);
		glActiveTexture(GL_TEXTURE11);
		glBindTexture(GL_TEXTURE_2D, chunk_grid_texture_);
		if (cull_chunk_grid_loc_ != -1)
			glUniform1i(cull_chunk_grid_loc_, 11);
		glActiveTexture(GL_TEXTURE12);
		glBindTexture(GL_TEXTURE_2D, max_height_grid_texture_);
		if (cull_max_height_grid_loc_ != -1)
			glUniform1i(cull_max_height_grid_loc_, 12);
		glActiveTexture(GL_TEXTURE13);
		glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture_);
		if (cull_heightmap_array_loc_ != -1)
			glUniform1i(cull_heightmap_array_loc_, 13);

		glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::TerrainData(), terrain_data_ubo_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, chunk_metadata_ssbo_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, patch_indices_ssbo_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, dynamic_ebo_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::TerrainIndirect(), indirect_buffer_);

		glDispatchCompute((next_gpu_index_ * 64 + 63) / 64, 1, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT | GL_ELEMENT_ARRAY_BARRIER_BIT);
	}

	void TerrainRenderManager::UpdateGridTextures(float world_scale) {
		int   grid_size = Constants::Class::Terrain::SliceMapSize();
		int   half_grid = grid_size / 2;
		float scaled_chunk_size = chunk_size_ * world_scale;
		int   center_chunk_x = static_cast<int>(std::floor(last_camera_pos_.x / scaled_chunk_size));
		int   center_chunk_z = static_cast<int>(std::floor(last_camera_pos_.z / scaled_chunk_size));
		int   origin_x = center_chunk_x - half_grid;
		int   origin_z = center_chunk_z - half_grid;

		bool grid_moved = (origin_x != last_grid_origin_x_ || origin_z != last_grid_origin_z_);
		bool scale_changed = (world_scale != last_grid_world_scale_);

		if (grid_moved || scale_changed || chunk_metadata_dirty_) {
			std::vector<int16_t> slice_data(grid_size * grid_size, -1);
			std::vector<float>   height_data(grid_size * grid_size, -10000.0f);
			for (const auto& [key, chunk] : chunks_) {
				int lx = key.first - origin_x;
				int lz = key.second - origin_z;
				if (lx >= 0 && lx < grid_size && lz >= 0 && lz < grid_size) {
					int idx = lz * grid_size + lx;
					// Note: We're using base_vertex / vertices_per_chunk as a virtual "slice" for grid lookup
					slice_data[idx] = static_cast<int16_t>(chunk.base_vertex / vertices_per_chunk_);
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

		glActiveTexture(GL_TEXTURE14);
		glBindTexture(GL_TEXTURE_2D_ARRAY, biome_texture_);
		shader_base.trySetInt("u_biomeMap", 14);

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
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		if (chunks_.empty() || mesh_vao_ == 0)
			return;
		shader.use();
		shader.setMat4("view", view);
		shader.setMat4("projection", projection);
		shader.setVec2("uViewportSize", viewport_size);
		shader.setMat4("model", glm::mat4(1.0f));
		shader.setFloat("uTessQualityMultiplier", tess_quality_multiplier);
		shader.setFloat("uChunkSize", chunk_size_ * last_world_scale_);
		if (clip_plane)
			shader.setVec4("clipPlane", *clip_plane);
		else
			shader.setVec4("clipPlane", glm::vec4(0, 0, 0, 0));

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture_);
		shader.setInt("uHeightmap", 0);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D_ARRAY, biome_texture_);
		shader.setInt("uBiomeMap", 1);
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
		glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::Biomes(), biome_ubo_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, chunk_metadata_ssbo_);

		glBindVertexArray(mesh_vao_);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirect_buffer_);

		glDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr);

		glBindVertexArray(0);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
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
		std::vector<glm::vec4>                result;
		result.reserve(chunks_.size());
		for (const auto& [key, chunk] : chunks_) {
			result.push_back(
				glm::vec4(
					chunk.world_offset.x,
					chunk.world_offset.y,
					static_cast<float>(chunk.base_vertex / vertices_per_chunk_),
					static_cast<float>(chunk_size_ * world_scale)
				)
			);
		}
		return result;
	}

	std::vector<TerrainRenderManager::DecorChunkData> TerrainRenderManager::GetDecorChunkData(float world_scale) const {
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		std::vector<DecorChunkData>           result;
		result.reserve(chunks_.size());
		float scaled_chunk_size = static_cast<float>(chunk_size_ * world_scale);
		for (const auto& [key, chunk] : chunks_) {
			result.push_back(
				{key,
			     chunk.world_offset,
			     static_cast<float>(chunk.base_vertex / vertices_per_chunk_),
			     scaled_chunk_size,
			     chunk.update_count}
			);
		}
		return result;
	}
} // namespace Boidsish
