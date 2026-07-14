#include "terrain_render_manager.h"

#include <algorithm>
#include <cstring>
#include <iostream>

#include <glm/gtx/norm.hpp>

#include "biome_properties.h"
#include "constants.h"
#include "gpu_resource_registry.h"
#include "graphics.h" // For Frustum
#include "profiler.h"
#include "service_locator.h"
#include "shader.h"

namespace Boidsish {

	struct TerrainDataUbo {
		glm::ivec4 origin_size;    // x, z, size, is_bound (1)
		glm::vec4  terrain_params; // chunk_size, world_scale, unused, unused
	};

	TerrainRenderManager::TerrainRenderManager(ServiceLocator& /*loc*/, int chunk_size, int max_chunks):
		chunk_size_(chunk_size), max_chunks_(max_chunks), heightmap_resolution_(chunk_size + 1) {
		// Create Biome UBO
		glGenBuffers(1, &biome_ubo_);
		glBindBuffer(GL_UNIFORM_BUFFER, biome_ubo_);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(BiomeShaderProperties) * kBiomes.size(), nullptr, GL_STATIC_DRAW);

		std::vector<BiomeShaderProperties> shader_biomes;
		for (const auto& b : kBiomes) {
			BiomeShaderProperties sb;
			sb.albedo_roughness = glm::vec4(b.albedo, b.roughness);
			sb.params = glm::vec4(b.metallic, b.detailStrength, b.detailScale, b.noiseType);
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

		// Global terrain grid resources (Consolidated RG32F: R=slice, G=maxHeight)
		int grid_size = Constants::Class::Terrain::SliceMapSize();
		glGenTextures(1, &chunk_grid_texture_);
		glBindTexture(GL_TEXTURE_2D, chunk_grid_texture_);
		int grid_mips = 1 + static_cast<int>(std::floor(std::log2(grid_size)));
		glTexStorage2D(GL_TEXTURE_2D, grid_mips, GL_RG32F, grid_size, grid_size);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		grid_mip_shader_ = std::make_unique<ComputeShader>("shaders/terrain_hiz_generate.comp");
		height_mip_shader_ = std::make_unique<ComputeShader>("shaders/terrain_height_mip.comp");
		probe_compute_shader_ = std::make_unique<ComputeShader>("shaders/terrain_probes.comp");
		terrain_bake_shader_ = std::make_unique<ComputeShader>("shaders/terrain_bake.comp");
		terrain_mesh_bake_shader_ = std::make_unique<ComputeShader>("shaders/terrain_mesh_bake.comp");
		terrain_horizon_shader_ = std::make_unique<ComputeShader>("shaders/terrain_horizon_update.comp");
		terrain_shadow_map_shader_ = std::make_unique<ComputeShader>("shaders/terrain_shadow_map.comp");
		patch_metrics_shader_ = std::make_unique<ComputeShader>("shaders/terrain_patch_metrics.comp");
		patch_prepare_shader_ = std::make_unique<ComputeShader>("shaders/terrain_prepare.comp");

		glGenTextures(1, &terrain_shadow_map_texture_);
		glBindTexture(GL_TEXTURE_2D, terrain_shadow_map_texture_);
		// 8192x8192 R8 for terrain shadow map
		glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8, 8192, 8192);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		// Create SH probes SSBO
		glGenBuffers(1, &probe_ssbo_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, probe_ssbo_);
		// SH coefficient size is 9 * 16 bytes = 144 bytes per probe
		// We initialize with a "poison" value in the w-components to force an immediate refresh
		size_t             probe_count = grid_size * grid_size;
		std::vector<float> initial_data(probe_count * 36, -99999.0f);
		glBufferData(
			GL_SHADER_STORAGE_BUFFER,
			probe_count * 144,
			initial_data.data(),
			GL_DYNAMIC_DRAW
		);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		// Create BakeTasks SSBO
		glGenBuffers(1, &bake_ssbo_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, bake_ssbo_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, 1024 * sizeof(BakeTask), nullptr, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		// Patch SSBOs for grass placement
		size_t max_patches = max_chunks_ * Constants::Class::Terrain::PatchesPerChunk();

		glGenBuffers(1, &patch_metrics_ssbo_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, patch_metrics_ssbo_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, max_patches * sizeof(PatchMetrics), nullptr, GL_DYNAMIC_DRAW);

		glGenBuffers(1, &patch_visibility_ssbo_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, patch_visibility_ssbo_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, max_patches * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);

		// Initialize direct VBO/EBO/VAO drawing structures
		chunk_states_pb_ = std::make_unique<PersistentBuffer<ChunkState>>(GL_SHADER_STORAGE_BUFFER, max_chunks_, 3);
		terrain_indirect_pb_ = std::make_unique<PersistentBuffer<IndirectCommandBuffer>>(GL_DRAW_INDIRECT_BUFFER, 1, 3);

		glGenBuffers(1, &terrain_vbo_);
		glBindBuffer(GL_ARRAY_BUFFER, terrain_vbo_);
		glBufferData(GL_ARRAY_BUFFER, max_chunks_ * 1089 * sizeof(TerrainVertex), nullptr, GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		// Pre-generate static LOD indices
		std::vector<unsigned int> all_indices;
		auto gen_indices = [](int width) {
			std::vector<unsigned int> idxs;
			for (int y = 0; y < width - 1; ++y) {
				for (int x = 0; x < width - 1; ++x) {
					unsigned int i0 = y * width + x;
					unsigned int i1 = y * width + (x + 1);
					unsigned int i2 = (y + 1) * width + x;
					unsigned int i3 = (y + 1) * width + (x + 1);
					// Counter-Clockwise (CCW) winding order when looking from above (+Y)
					idxs.push_back(i0); idxs.push_back(i2); idxs.push_back(i1);
					idxs.push_back(i1); idxs.push_back(i2); idxs.push_back(i3);
				}
			}
			return idxs;
		};

		auto lod0 = gen_indices(33); // LOD 0 (32x32 quads) - count 6144
		auto lod1 = gen_indices(17); // LOD 1 (16x16 quads) - count 1536
		auto lod2 = gen_indices(9);  // LOD 2 (8x8 quads) - count 384
		auto lod3 = gen_indices(5);  // LOD 3 (4x4 quads) - count 96

		all_indices.insert(all_indices.end(), lod0.begin(), lod0.end());
		all_indices.insert(all_indices.end(), lod1.begin(), lod1.end());
		all_indices.insert(all_indices.end(), lod2.begin(), lod2.end());
		all_indices.insert(all_indices.end(), lod3.begin(), lod3.end());

		glGenBuffers(1, &terrain_ebo_);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, terrain_ebo_);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, all_indices.size() * sizeof(unsigned int), all_indices.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		glGenVertexArrays(1, &terrain_vao_);
		glBindVertexArray(terrain_vao_);
		glBindBuffer(GL_ARRAY_BUFFER, terrain_vbo_);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, terrain_ebo_);

		// layout(location = 0) in vec3 aPos;
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, position));
		glEnableVertexAttribArray(0);

		// layout(location = 1) in vec3 aNormal;
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, normal));
		glEnableVertexAttribArray(1);

		// layout(location = 2) in vec2 aTexCoords;
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, tex_coords));
		glEnableVertexAttribArray(2);

		// layout(location = 3) in float aTextureSlice;
		glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)(offsetof(TerrainVertex, position) + 3 * sizeof(float)));
		glEnableVertexAttribArray(3);

		// layout(location = 4) in float aPerturbFactor;
		glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)(offsetof(TerrainVertex, normal) + 3 * sizeof(float)));
		glEnableVertexAttribArray(4);

		// layout(location = 5) in float aTessFactor;
		glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)(offsetof(TerrainVertex, tex_coords) + 2 * sizeof(float)));
		glEnableVertexAttribArray(5);

		// layout(location = 6) in float aIsWater;
		glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)(offsetof(TerrainVertex, tex_coords) + 3 * sizeof(float)));
		glEnableVertexAttribArray(6);

		// layout(location = 7) in float aErosionDelta;
		glVertexAttribPointer(7, 1, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, params));
		glEnableVertexAttribArray(7);

		// layout(location = 8) in float aRidgeMap;
		glVertexAttribPointer(8, 1, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)(offsetof(TerrainVertex, params) + 1 * sizeof(float)));
		glEnableVertexAttribArray(8);

		// layout(location = 9) in float aSubstrate;
		glVertexAttribPointer(9, 1, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)(offsetof(TerrainVertex, params) + 2 * sizeof(float)));
		glEnableVertexAttribArray(9);

		glBindVertexArray(0);

		// Create instance buffer first so we can set up VAO attributes
		// Pre-allocate for max_chunks to avoid reallocation
		glGenBuffers(1, &instance_vbo_);
		glBindBuffer(GL_ARRAY_BUFFER, instance_vbo_);
		instance_buffer_capacity_ = max_chunks * sizeof(InstanceData);
		glBufferData(GL_ARRAY_BUFFER, instance_buffer_capacity_, nullptr, GL_DYNAMIC_DRAW);

		CreateGridMesh();

		EnsureTextureCapacity(max_chunks);

		auto& reg = GpuResourceRegistry::Instance();
		reg.PublishTexture(Constants::TextureUnit::TerrainChunkGrid(), chunk_grid_texture_);
		reg.PublishTexture(Constants::TextureUnit::TerrainShadowMap(), terrain_shadow_map_texture_);
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
		if (raw_heightmap_texture_)
			glDeleteTextures(1, &raw_heightmap_texture_);
		if (heightmap_texture_)
			glDeleteTextures(1, &heightmap_texture_);
		if (baked_params_texture_)
			glDeleteTextures(1, &baked_params_texture_);
		if (displacement_texture_)
			glDeleteTextures(1, &displacement_texture_);
		if (horizon_map_texture_)
			glDeleteTextures(1, &horizon_map_texture_);
		if (terrain_shadow_map_texture_)
			glDeleteTextures(1, &terrain_shadow_map_texture_);
		if (biome_texture_)
			glDeleteTextures(1, &biome_texture_);
		if (biome_ubo_)
			glDeleteBuffers(1, &biome_ubo_);
		if (chunk_grid_texture_)
			glDeleteTextures(1, &chunk_grid_texture_);
		if (terrain_data_ubo_)
			glDeleteBuffers(1, &terrain_data_ubo_);
		if (probe_ssbo_)
			glDeleteBuffers(1, &probe_ssbo_);
		if (bake_ssbo_)
			glDeleteBuffers(1, &bake_ssbo_);
		if (patch_metrics_ssbo_)
			glDeleteBuffers(1, &patch_metrics_ssbo_);
		if (patch_visibility_ssbo_)
			glDeleteBuffers(1, &patch_visibility_ssbo_);

		chunk_states_pb_.reset();
		terrain_indirect_pb_.reset();

		for (int i = 0; i < 3; ++i) {
			if (patch_fences_[i]) {
				glDeleteSync(patch_fences_[i]);
			}
		}
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
		if (raw_heightmap_texture_ && heightmap_texture_ && baked_params_texture_ && displacement_texture_ && biome_texture_ && required_slices <= max_chunks_) {
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

		size_t max_patches = new_capacity * Constants::Class::Terrain::PatchesPerChunk();

		// Resize patch SSBOs
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, patch_metrics_ssbo_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, max_patches * sizeof(PatchMetrics), nullptr, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, patch_visibility_ssbo_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, max_patches * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);

		// Resize prebaked VBO
		if (terrain_vbo_) {
			glDeleteBuffers(1, &terrain_vbo_);
		}
		glGenBuffers(1, &terrain_vbo_);
		glBindBuffer(GL_ARRAY_BUFFER, terrain_vbo_);
		glBufferData(GL_ARRAY_BUFFER, new_capacity * 1089 * sizeof(TerrainVertex), nullptr, GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		// Update VAO vertex buffer binding
		if (terrain_vao_) {
			glBindVertexArray(terrain_vao_);
			glBindBuffer(GL_ARRAY_BUFFER, terrain_vbo_);
			// Pos
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, position));
			// Norm
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, normal));
			// UV
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, tex_coords));
			// Slice
			glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)(offsetof(TerrainVertex, position) + 3 * sizeof(float)));
			// Perturb
			glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)(offsetof(TerrainVertex, normal) + 3 * sizeof(float)));
			// Tess
			glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)(offsetof(TerrainVertex, tex_coords) + 2 * sizeof(float)));
			// IsWater
			glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)(offsetof(TerrainVertex, tex_coords) + 3 * sizeof(float)));
			// Erosion
			glVertexAttribPointer(7, 1, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)offsetof(TerrainVertex, params));
			// Ridge
			glVertexAttribPointer(8, 1, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)(offsetof(TerrainVertex, params) + 1 * sizeof(float)));
			// Substrate
			glVertexAttribPointer(9, 1, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), (void*)(offsetof(TerrainVertex, params) + 2 * sizeof(float)));
			glBindVertexArray(0);
		}

		chunk_states_pb_ = std::make_unique<PersistentBuffer<ChunkState>>(GL_SHADER_STORAGE_BUFFER, new_capacity, 3);
		terrain_indirect_pb_ = std::make_unique<PersistentBuffer<IndirectCommandBuffer>>(GL_DRAW_INDIRECT_BUFFER, 1, 3);

		// If we need to resize and texture exists, existing data will be lost
		// This shouldn't happen often with proper capacity management
		if (raw_heightmap_texture_) {
			std::cerr << "[TerrainRenderManager] WARNING: Texture array resize from " << max_chunks_ << " to "
			          << new_capacity << " - existing heightmap data will be lost!" << std::endl;
			glDeleteTextures(1, &raw_heightmap_texture_);
			raw_heightmap_texture_ = 0;
			glDeleteTextures(1, &heightmap_texture_);
			heightmap_texture_ = 0;
			glDeleteTextures(1, &baked_params_texture_);
			baked_params_texture_ = 0;
			glDeleteTextures(1, &displacement_texture_);
			displacement_texture_ = 0;

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

		auto create_array = [&](GLuint& tex, GLenum internalFormat, GLenum format, GLenum type, bool linear, bool mips = false) {
			glGenTextures(1, &tex);
			glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
			int levels = mips ? (1 + static_cast<int>(std::floor(std::log2(heightmap_resolution_)))) : 1;
			glTexStorage3D(GL_TEXTURE_2D_ARRAY, levels, internalFormat, heightmap_resolution_, heightmap_resolution_, max_chunks_);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, linear ? (mips ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR) : GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		};

		create_array(raw_heightmap_texture_, GL_RGBA16F, GL_RGBA, GL_FLOAT, true);
		create_array(heightmap_texture_, GL_RGBA16F, GL_RGBA, GL_FLOAT, true, true);
		create_array(baked_params_texture_, GL_RGBA16F, GL_RGBA, GL_FLOAT, true);
		create_array(displacement_texture_, GL_RGBA16F, GL_RGBA, GL_FLOAT, true);

		// Horizon map: 8 directions (8x8 resolution per chunk)
		glGenTextures(1, &horizon_map_texture_);
		glBindTexture(GL_TEXTURE_2D_ARRAY, horizon_map_texture_);
		glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA16F, 8, 8, max_chunks_, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		create_array(biome_texture_, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, true);

		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

		auto& reg = GpuResourceRegistry::Instance();
		reg.PublishTexture(Constants::TextureUnit::TerrainHeightmap(), heightmap_texture_, GL_TEXTURE_2D_ARRAY);
	}

	void TerrainRenderManager::UploadHeightmapSlice(
		int                        slice,
		const std::vector<float>&  packed_height_normal,
		const std::vector<uint8_t>& packed_biomes
	) {
		glBindTexture(GL_TEXTURE_2D_ARRAY, raw_heightmap_texture_);
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
			packed_height_normal.data()
		);

		// Also upload to heightmap_texture_ as a fallback until baking is complete
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
			packed_height_normal.data()
		);

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
			GL_RGBA,
			GL_UNSIGNED_BYTE,
			packed_biomes.data()
		);

		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	}

	void TerrainRenderManager::InitializeSliceData(int slice, float min_y, float max_y) {
		// 1. Clear baked textures to prevent ghosting from previous chunks
		float clear_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		glClearTexSubImage(baked_params_texture_, 0, 0, 0, slice, heightmap_resolution_, heightmap_resolution_, 1, GL_RGBA, GL_FLOAT, clear_color);
		glClearTexSubImage(displacement_texture_, 0, 0, 0, slice, heightmap_resolution_, heightmap_resolution_, 1, GL_RGBA, GL_FLOAT, clear_color);

		// Initialize heightmap mips 1+ with conservative values (Max=10000, Min=-10000)
		// This ensures hierarchical raymarching doesn't skip unbaked chunks.
		float conservative_min_max[4] = {10000.0f, -10000.0f, 0.0f, 0.0f};
		int levels = 1 + static_cast<int>(std::floor(std::log2(heightmap_resolution_)));
		for (int mip = 1; mip < levels; ++mip) {
			int mip_res = std::max(1, heightmap_resolution_ >> mip);
			glClearTexSubImage(heightmap_texture_, mip, 0, 0, slice, mip_res, mip_res, 1, GL_RGBA, GL_FLOAT, conservative_min_max);
		}

		// Clear horizon map (8x8)
		glClearTexSubImage(horizon_map_texture_, 0, 0, 0, slice, 8, 8, 1, GL_RGBA, GL_FLOAT, clear_color);

		// 2. Initialize patch metrics with conservative bounds
		int num_patches_per_chunk = Constants::Class::Terrain::PatchesPerChunk();
		std::vector<PatchMetrics> initial_metrics(num_patches_per_chunk);
		for (auto& m : initial_metrics) {
			m.min_y = min_y - 2.0f;
			m.max_y = max_y + 2.0f;
			m.avg_curvature = 0.0f;
			m.avg_roughness = 0.0f;
			m.avg_grass_density = 0.0f;
			m.max_variance = 0.0f;
		}

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, patch_metrics_ssbo_);
		glBufferSubData(
			GL_SHADER_STORAGE_BUFFER,
			slice * num_patches_per_chunk * sizeof(PatchMetrics),
			num_patches_per_chunk * sizeof(PatchMetrics),
			initial_metrics.data()
		);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}

	void TerrainRenderManager::RegisterChunk(
		std::pair<int, int>              chunk_key,
		const std::vector<glm::vec3>&    positions,
		const std::vector<glm::vec3>&    normals,
		const std::vector<glm::vec2>&    biomes,
		const std::vector<float>&        packed_height_normal,
		const std::vector<uint8_t>&      packed_biomes,
		const std::vector<unsigned int>& indices, // Not used in this implementation
		float                            min_y,
		float                            max_y,
		const glm::vec3&                 world_offset,
		float                            world_scale
	) {
		// Deferred eviction callback to avoid deadlock
		// (caller may hold terrain generator's mutex, and callback needs that mutex)
		bool                should_notify_eviction = false;
		std::pair<int, int> evicted_chunk_key;

		// Update world scale tracking
		last_world_scale_ = world_scale;

		// Scoped lock - released before calling eviction callback to avoid deadlock
		{
			std::lock_guard<std::recursive_mutex> lock(mutex_);

			// If chunk already exists, update it
			auto it = chunks_.find(chunk_key);
			if (it != chunks_.end()) {
				// Update existing chunk's heightmap
				UploadHeightmapSlice(it->second.texture_slice, packed_height_normal, packed_biomes);
				InitializeSliceData(it->second.texture_slice, min_y, max_y);
				it->second.min_y = min_y;
				it->second.max_y = max_y;
				it->second.update_count++;
				grid_dirty_ = true;
		needs_prep_ = true;

				// Queue for baking
				bake_queue_.push_back({glm::ivec2(chunk_key.first, chunk_key.second), it->second.texture_slice, 0});
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
			UploadHeightmapSlice(slice, packed_height_normal, packed_biomes);
			InitializeSliceData(slice, min_y, max_y);

			// Queue for baking
			bake_queue_.push_back({glm::ivec2(chunk_key.first, chunk_key.second), slice, 0});

			// Store chunk info
			ChunkInfo info{};
			info.texture_slice = slice;
			info.min_y = min_y;
			info.max_y = max_y;
			info.world_offset = glm::vec2(world_offset.x, world_offset.z);

			chunks_[chunk_key] = info;
			grid_dirty_ = true;
			needs_prep_ = true;
		} // mutex released here

		// Call eviction callback outside the lock to avoid deadlock
		if (should_notify_eviction && eviction_callback_) {
			eviction_callback_(evicted_chunk_key);
		}
	}

	void TerrainRenderManager::UnregisterChunk(std::pair<int, int> chunk_key) {
		std::lock_guard<std::recursive_mutex> lock(mutex_);

		auto it = chunks_.find(chunk_key);
		if (it == chunks_.end()) {
			return;
		}

		// Return slice to free list
		free_slices_.push_back(it->second.texture_slice);
		chunks_.erase(it);
		grid_dirty_ = true;
		needs_prep_ = true;
	}

	bool TerrainRenderManager::HasChunk(std::pair<int, int> chunk_key) const {
		std::lock_guard<std::recursive_mutex> lock(mutex_);
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

		// Add a small safety margin (10% of chunk size) to prevent edge flickering
		float margin = scaled_chunk_size * 0.1f;

		glm::vec3 center = (min_corner + max_corner) * 0.5f;
		glm::vec3 half_size = (max_corner - min_corner) * 0.5f + glm::vec3(margin, margin, margin);

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

	void TerrainRenderManager::PrepareForRender(
		const Frustum&   frustum,
		const glm::vec3& camera_pos,
		float            world_scale,
		GLuint           lighting_ubo,
		GLintptr         lighting_ubo_offset,
		GLsizeiptr       lighting_ubo_size,
		float            day_time,
		const glm::vec3& sun_dir,
		GLintptr         temporal_ubo_offset,
		GLintptr         frustum_ubo_offset,
		float            lod_projection_scalar
	) {
		PROJECT_PROFILE_SCOPE("TerrainRenderManager::PrepareForRender");

		std::lock_guard<std::recursive_mutex> lock(mutex_);

		temporal_data_ubo_offset_ = temporal_ubo_offset;
		frustum_ubo_offset_ = frustum_ubo_offset;

		// Store camera position and world scale for LRU eviction decisions in RegisterChunk
		last_camera_pos_ = camera_pos;
		last_world_scale_ = world_scale;

		UpdateGridTextures(world_scale);

		// Perform baking of any pending chunks FIRST so their current_lod is fresh
		PerformBaking(world_scale);

		// Advance double/triple buffers before checking/writing
		chunk_states_pb_->AdvanceFrame();
		terrain_indirect_pb_->AdvanceFrame();

		// Clear indirect draw count for this frame
		IndirectCommandBuffer* mapped_indirect = terrain_indirect_pb_->GetFrameDataPtr();
		mapped_indirect->draw_count = 0;

		// Compute LODs and flag for rebake
		auto* states_ptr = chunk_states_pb_->GetFrameDataPtr();
		// Reset/zero states_ptr to prevent any garbage values
		std::memset(states_ptr, 0, max_chunks_ * sizeof(ChunkState));

		glm::vec2 camera_pos_2d(camera_pos.x, camera_pos.z);

		for (auto& [key, chunk] : chunks_) {
			float     scaled_chunk_size = chunk_size_ * world_scale;
			glm::vec2 chunk_center(
				chunk.world_offset.x + scaled_chunk_size * 0.5f,
				chunk.world_offset.y + scaled_chunk_size * 0.5f
			);
			float dist = glm::distance(camera_pos_2d, chunk_center);

			// Determine target LOD with hysteresis to prevent thrashing visual disruption
			int target_lod = chunk.current_lod;
			if (target_lod == -1) {
				if (dist < 128.0f * world_scale) target_lod = 0;
				else if (dist < 256.0f * world_scale) target_lod = 1;
				else if (dist < 512.0f * world_scale) target_lod = 2;
				else target_lod = 3;
			} else {
				if (target_lod == 0 && dist > 144.0f * world_scale) target_lod = 1;
				else if (target_lod == 1 && dist < 112.0f * world_scale) target_lod = 0;
				else if (target_lod == 1 && dist > 288.0f * world_scale) target_lod = 2;
				else if (target_lod == 2 && dist < 224.0f * world_scale) target_lod = 1;
				else if (target_lod == 2 && dist > 544.0f * world_scale) target_lod = 3;
				else if (target_lod == 3 && dist < 480.0f * world_scale) target_lod = 2;
			}

			// If current baked LOD differs from target LOD, trigger a rebake task
			if (chunk.current_lod != target_lod) {
				// Avoid double-queuing if already in bake_queue_
				bool already_queued = false;
				for (const auto& task : bake_queue_) {
					if (task.chunk_coord == glm::ivec2(key.first, key.second)) {
						already_queued = true;
						break;
					}
				}
				if (!already_queued) {
					bake_queue_.push_back({glm::ivec2(key.first, key.second), chunk.texture_slice, target_lod});
				}
			}

			// Pre-generate state offsets for index/vertex sizes based on chunk.current_lod
			uint32_t index_offset = 0;
			uint32_t index_count = 0;
			uint32_t vertex_count = 0;
			switch (chunk.current_lod) {
				case 0: index_offset = 0; index_count = 6144; vertex_count = 1089; break;
				case 1: index_offset = 6144; index_count = 1536; vertex_count = 289; break;
				case 2: index_offset = 7680; index_count = 384; vertex_count = 81; break;
				case 3: index_offset = 8064; index_count = 96; vertex_count = 25; break;
				default: index_offset = 0; index_count = 0; vertex_count = 0; break;
			}

			ChunkState& state = states_ptr[chunk.texture_slice];
			state.coordinate = glm::ivec2(key.first, key.second);
			state.is_visible = IsChunkVisible(chunk, frustum, world_scale) ? 1 : 0;
			state.current_lod = chunk.current_lod;
			state.target_lod = target_lod;
			state.needs_rebake = (chunk.current_lod != target_lod) ? 1 : 0;
			state.vertex_offset = chunk.texture_slice * 1089;
			state.vertex_count = vertex_count;
			state.index_offset = index_offset;
			state.index_count = index_count;
			state.last_frame_used = frame_count_;
		}

		// Update shadow map if needed
		if (glm::length(sun_dir) > 0.1f) {
			UpdateTerrainShadowMap(sun_dir, world_scale);
		}

		visible_instances_.clear();
		visible_instances_.reserve(chunks_.size());

		// Collect visible chunks with distance info
		struct VisibleChunk {
			InstanceData instance;
			float        distance_sq;
		};

		std::vector<VisibleChunk> visible_chunks;
		visible_chunks.reserve(chunks_.size());

		{
			PROJECT_PROFILE_SCOPE("VisibilityCulling");
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
		}

		// Sort by distance (front-to-back for better early-Z rejection)
		{
			PROJECT_PROFILE_SCOPE("SortChunks");
			std::sort(visible_chunks.begin(), visible_chunks.end(), [](const VisibleChunk& a, const VisibleChunk& b) {
				return a.distance_sq < b.distance_sq;
			});
		}

		// Build final instance list
		for (const auto& vc : visible_chunks) {
			visible_instances_.push_back(vc.instance);
		}

		// Upload instance data to GPU for the prepare compute shader
		PROJECT_PROFILE_SCOPE("UploadInstanceData");
		if (!visible_instances_.empty()) {
			// Update bounds with raw chunkSize for the prepare shader's UV calculation
			for (auto& instance : visible_instances_) {
				instance.bounds.z = static_cast<float>(chunk_size_);
			}

			// Use internal instance_vbo_ but bind as SSBO for prepare shader
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, instance_vbo_);
			size_t required_size = visible_instances_.size() * sizeof(InstanceData);
			if (required_size > instance_buffer_capacity_) {
				instance_buffer_capacity_ = required_size * 2;
				glBufferData(GL_SHADER_STORAGE_BUFFER, instance_buffer_capacity_, nullptr, GL_DYNAMIC_DRAW);
			}
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, required_size, visible_instances_.data());
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		}
	}

	void TerrainRenderManager::UpdateGridTextures(float world_scale) {
		PROJECT_PROFILE_SCOPE("TerrainRenderManager::UpdateGridTextures");
		int grid_size = Constants::Class::Terrain::SliceMapSize();
		int half_grid = grid_size / 2;

		float scaled_chunk_size = chunk_size_ * world_scale;
		int   center_chunk_x = static_cast<int>(std::floor(last_camera_pos_.x / scaled_chunk_size));
		int   center_chunk_z = static_cast<int>(std::floor(last_camera_pos_.z / scaled_chunk_size));

		int origin_x = center_chunk_x - half_grid;
		int origin_z = center_chunk_z - half_grid;

		if (origin_x == last_grid_origin_x_ && origin_z == last_grid_origin_z_ &&
		    world_scale == last_grid_world_scale_ && !grid_dirty_) {
			return;
		}

		std::vector<glm::vec2> grid_data(grid_size * grid_size, glm::vec2(-1.0f, -10000.0f));

		for (const auto& [key, chunk] : chunks_) {
			int lx = key.first - origin_x;
			int lz = key.second - origin_z;

			if (lx >= 0 && lx < grid_size && lz >= 0 && lz < grid_size) {
				int idx = lz * grid_size + lx;
				grid_data[idx].x = static_cast<float>(chunk.texture_slice);
				// Add a vertical safety buffer to account for dynamic terrain displacements
				// (erosion, shockwaves, micro-relief) in the Hi-Z structure.
				grid_data[idx].y = chunk.max_y + (5.0f * world_scale);
			}
		}

		glBindTexture(GL_TEXTURE_2D, chunk_grid_texture_);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, grid_size, grid_size, GL_RG, GL_FLOAT, grid_data.data());

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
		grid_dirty_ = false;
	}

	void TerrainRenderManager::DispatchProbeUpdate(
		GLuint           colorTex,
		GLuint           depthTex,
		GLuint           normalTex,
		GLuint           albedoTex,
		GLuint           velocityTex,
		GLuint           skyLUT,
		GLuint           apLUT,
		const glm::mat4& view,
		const glm::mat4& projection,
		GLuint           lighting_ubo,
		GLintptr         lighting_ubo_offset,
		GLsizeiptr       lighting_ubo_size,
		float            probe_scaling,
		float            probe_convergence,
		int              probe_ray_multiplier
	) {
		PROJECT_PROFILE_SCOPE("TerrainRenderManager::DispatchProbeUpdate");
		std::lock_guard<std::recursive_mutex> lock(mutex_);

		if (!probe_compute_shader_ || !probe_compute_shader_->isValid())
			return;

		int grid_size = Constants::Class::Terrain::SliceMapSize();

		probe_compute_shader_->use();
		frame_count_++;

		// Bind textures
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, colorTex);
		probe_compute_shader_->setInt("u_gColor", 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depthTex);
		probe_compute_shader_->setInt("u_gDepth", 1);

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, normalTex);
		probe_compute_shader_->setInt("u_gNormal", 2);

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, albedoTex);
		probe_compute_shader_->setInt("u_gAlbedo", 3);

		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, velocityTex);
		probe_compute_shader_->setInt("u_gVelocity", 4);

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::AtmosphereSkyView());
		glBindTexture(GL_TEXTURE_2D, skyLUT);
		probe_compute_shader_->setInt("u_skyViewLUT", Constants::TextureUnit::AtmosphereSkyView());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::AtmosphereAerialPerspective());
		glBindTexture(GL_TEXTURE_3D, apLUT);
		probe_compute_shader_->setInt("u_aerialPerspectiveLUT", Constants::TextureUnit::AtmosphereAerialPerspective());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainBiomeMap());
		glBindTexture(GL_TEXTURE_2D_ARRAY, biome_texture_);
		probe_compute_shader_->setInt("u_biomeMap", Constants::TextureUnit::TerrainBiomeMap());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainChunkGrid());
		glBindTexture(GL_TEXTURE_2D, chunk_grid_texture_);
		probe_compute_shader_->setInt("u_chunkGrid", Constants::TextureUnit::TerrainChunkGrid());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainHeightmap());
		glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture_);
		probe_compute_shader_->setInt("u_heightmapArray", Constants::TextureUnit::TerrainHeightmap());


		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainShadowMap());
		glBindTexture(GL_TEXTURE_2D, terrain_shadow_map_texture_);
		probe_compute_shader_->setInt("u_terrainShadowMap", Constants::TextureUnit::TerrainShadowMap());

		// Uniforms
		probe_compute_shader_->setUint("u_frameIndex", frame_count_);
		probe_compute_shader_->setMat4("u_view", view);
		probe_compute_shader_->setMat4("u_projection", projection);
		probe_compute_shader_->setMat4("u_invView", glm::inverse(view));
		probe_compute_shader_->setMat4("u_invProjection", glm::inverse(projection));

		probe_compute_shader_->setFloat("u_probeScaling", probe_scaling);
		probe_compute_shader_->setFloat("u_probeConvergenceSpeed", probe_convergence);
		probe_compute_shader_->setInt("u_probeRayMultiplier", probe_ray_multiplier);

		glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::TerrainData(), terrain_data_ubo_);
		glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::Biomes(), biome_ubo_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::TerrainProbes(), probe_ssbo_);

		if (lighting_ubo != 0) {
			if (lighting_ubo_size > 0) {
				glBindBufferRange(GL_UNIFORM_BUFFER, Constants::UboBinding::Lighting(),
					lighting_ubo, lighting_ubo_offset, lighting_ubo_size);
			} else {
				glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::Lighting(), lighting_ubo);
			}
		}

		glDispatchCompute((grid_size + 7) / 8, (grid_size + 7) / 8, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
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

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, chunk_grid_texture_);
			grid_mip_shader_->setInt("u_srcDepth", 0);
			grid_mip_shader_->setInt("u_srcLevel", mip - 1);

			glBindImageTexture(0, chunk_grid_texture_, mip, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG32F);

			glDispatchCompute((dst_w + 7) / 8, (dst_h + 7) / 8, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
		}

		glBindTexture(GL_TEXTURE_2D, chunk_grid_texture_);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mips - 1);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void TerrainRenderManager::BindTerrainData(ShaderBase& shader_base) const {
		shader_base.use();
		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainChunkGrid());
		glBindTexture(GL_TEXTURE_2D, chunk_grid_texture_);
		shader_base.setInt("u_chunkGrid", Constants::TextureUnit::TerrainChunkGrid());


		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainHeightmap());
		glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture_);
		shader_base.trySetInt("u_heightmapArray", Constants::TextureUnit::TerrainHeightmap());
		shader_base.trySetInt("uHeightmap", Constants::TextureUnit::TerrainHeightmap());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainBiomeMap());
		glBindTexture(GL_TEXTURE_2D_ARRAY, biome_texture_);
		shader_base.trySetInt("uBiomeMap", Constants::TextureUnit::TerrainBiomeMap());
		shader_base.trySetInt("u_biomeMap", Constants::TextureUnit::TerrainBiomeMap());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainBakedParams());
		glBindTexture(GL_TEXTURE_2D_ARRAY, baked_params_texture_);
		shader_base.trySetInt("uBakedParams", Constants::TextureUnit::TerrainBakedParams());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainDisplacement());
		glBindTexture(GL_TEXTURE_2D_ARRAY, displacement_texture_);
		shader_base.trySetInt("u_displacementArray", Constants::TextureUnit::TerrainDisplacement());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainHorizonMap());
		glBindTexture(GL_TEXTURE_2D_ARRAY, horizon_map_texture_);
		shader_base.trySetInt("u_terrainHorizonMap", Constants::TextureUnit::TerrainHorizonMap());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainShadowMap());
		glBindTexture(GL_TEXTURE_2D, terrain_shadow_map_texture_);
		shader_base.trySetInt("u_terrainShadowMap", Constants::TextureUnit::TerrainShadowMap());

		if (extra_noise_texture_ != 0) {
			glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::NoiseExtra());
			glBindTexture(GL_TEXTURE_3D, extra_noise_texture_);
			shader_base.setInt("u_extraNoiseTexture", Constants::TextureUnit::NoiseExtra());
		}

		if (blue_noise_texture_ != 0) {
			glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::NoiseBlue());
			glBindTexture(GL_TEXTURE_2D, blue_noise_texture_);
			shader_base.trySetInt("u_blueNoiseTexture", Constants::TextureUnit::NoiseBlue());
		}

		if (phasor_noise_texture_ != 0) {
			glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::NoisePhasor());
			glBindTexture(GL_TEXTURE_2D, phasor_noise_texture_);
			shader_base.trySetInt("u_phasorTexture", Constants::TextureUnit::NoisePhasor());
		}

		glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::TerrainData(), terrain_data_ubo_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::TerrainProbes(), probe_ssbo_);

		if (grass_props_ubo_ != 0) {
			glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::GrassProps(), grass_props_ubo_);
		}
	}

	void TerrainRenderManager::DispatchPreparePatches(float tess_quality_multiplier, const glm::vec2& viewport_size, float lod_projection_scalar) {
		PROJECT_PROFILE_SCOPE("TerrainRenderManager::DispatchPreparePatches");
		std::lock_guard<std::recursive_mutex> lock(mutex_);

		if (visible_instances_.empty() || !patch_prepare_shader_ || !patch_prepare_shader_->isValid()) {
			return;
		}

		int current_idx = chunk_states_pb_->GetCurrentBufferIndex();
		if (patch_fences_[current_idx]) {
			glClientWaitSync(patch_fences_[current_idx], GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
			glDeleteSync(patch_fences_[current_idx]);
			patch_fences_[current_idx] = 0;
		}

		// Clear patch visibility buffer for grass placement
		size_t max_patches = max_chunks_ * Constants::Class::Terrain::PatchesPerChunk();
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, patch_visibility_ssbo_);
		glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);

		patch_prepare_shader_->use();
		BindTerrainData(*patch_prepare_shader_);

		// Bind structures for prebaked draw culling
		chunk_states_pb_->BindRange(Constants::SsboBinding::TerrainPatchTessLevels());

		// Bind indirect buffer as GL_SHADER_STORAGE_BUFFER so compute shader can write to it
		glBindBufferRange(
			GL_SHADER_STORAGE_BUFFER,
			Constants::SsboBinding::TerrainPatchIndirect(),
			terrain_indirect_pb_->GetBufferId(),
			terrain_indirect_pb_->GetFrameOffset(),
			terrain_indirect_pb_->GetTotalSize() / 3
		);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::TerrainPatchVisibility(), patch_visibility_ssbo_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::TerrainPatchMetrics(), patch_metrics_ssbo_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::IndirectionBuffer(), instance_vbo_);

		if (temporal_data_ubo_ != 0) {
			if (temporal_data_ubo_size_ > 0) {
				glBindBufferRange(GL_UNIFORM_BUFFER, Constants::UboBinding::TemporalData(),
					temporal_data_ubo_, temporal_data_ubo_offset_, temporal_data_ubo_size_);
			} else {
				glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::TemporalData(), temporal_data_ubo_);
			}
		}
		if (frustum_ubo_ != 0) {
			if (frustum_ubo_size_ > 0) {
				glBindBufferRange(GL_UNIFORM_BUFFER, Constants::UboBinding::FrustumData(),
					frustum_ubo_, frustum_ubo_offset_, frustum_ubo_size_);
			} else {
				glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::FrustumData(), frustum_ubo_);
			}
		}

		patch_prepare_shader_->setInt("u_numChunks", static_cast<int>(visible_instances_.size()));
		patch_prepare_shader_->setFloat("u_worldScale", last_world_scale_);
		patch_prepare_shader_->setVec3("u_viewPos", last_camera_pos_);

		// Bind Hi-Z and set occlusion uniforms if enabled
		auto& reg = GpuResourceRegistry::Instance();
		GLuint hizTex = reg.GetTexture(Constants::TextureUnit::HiZ());
		if (hizTex != 0) {
			glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::HiZ());
			glBindTexture(GL_TEXTURE_2D, hizTex);
			patch_prepare_shader_->setInt("u_hizTexture", Constants::TextureUnit::HiZ());

			GLint w, h, mips;
			glGetTextureLevelParameteriv(hizTex, 0, GL_TEXTURE_WIDTH, &w);
			glGetTextureLevelParameteriv(hizTex, 0, GL_TEXTURE_HEIGHT, &h);
			mips = 1 + static_cast<GLint>(std::floor(std::log2(std::max(w, h))));

			patch_prepare_shader_->setIVec2("u_hizSize", w, h);
			patch_prepare_shader_->setInt("u_hizMipCount", mips);
			patch_prepare_shader_->setFloat("u_screenExpansion", 4.0f);
			patch_prepare_shader_->setBool("u_enableOcclusionCulling", true);
		} else {
			patch_prepare_shader_->setBool("u_enableOcclusionCulling", false);
		}

		// Run prepare shader - one thread per visible/active chunk
		glDispatchCompute((static_cast<int>(visible_instances_.size()) + 63) / 64, 1, 1);
		glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

		patch_fences_[current_idx] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

		last_prep_camera_pos_ = last_camera_pos_;
		last_prep_world_scale_ = last_world_scale_;
		last_prep_tess_multiplier_ = tess_quality_multiplier;
		last_prep_visible_chunk_count_ = visible_instances_.size();
		needs_prep_ = false;
	}

	void TerrainRenderManager::Render(
		Shader&                         shader,
		const glm::mat4&                view,
		const glm::mat4&                projection,
		const glm::vec2&                viewport_size,
		const std::optional<glm::vec4>& clip_plane,
		float                           tess_quality_multiplier
	) {
		PROJECT_PROFILE_SCOPE("TerrainRenderManager::Render");
		std::lock_guard<std::recursive_mutex> lock(mutex_);

		if (visible_instances_.empty() || terrain_vao_ == 0) {
			return;
		}

		// GPU preparation should have already been dispatched in Visualizer::Render
		// to make visibility results available for the grass system.
		if (needs_prep_) {
			float fallback_scalar = viewport_size.y / (2.0f * 1.0f); // tan(45) = 1
			DispatchPreparePatches(tess_quality_multiplier, viewport_size, fallback_scalar);
		}

		// Render visible chunks directly
		shader.use();
		shader.setMat4("view", view);
		shader.setMat4("projection", projection);
		shader.setVec2("uViewportSize", viewport_size);
		shader.setMat4("model", glm::mat4(1.0f));
		shader.setFloat("uTessQualityMultiplier", tess_quality_multiplier);
		shader.setFloat("uChunkSize", chunk_size_ * last_world_scale_);
		shader.setFloat("uRawChunkSize", static_cast<float>(chunk_size_));

		if (clip_plane) {
			shader.setVec4("clipPlane", *clip_plane);
		} else {
			shader.setVec4("clipPlane", glm::vec4(0, 0, 0, 0));
		}

		// Bind textures for the fragment/vertex shaders
		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainHeightmap());
		glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture_);
		shader.trySetInt("uHeightmap", Constants::TextureUnit::TerrainHeightmap());
		shader.trySetInt("u_heightmapArray", Constants::TextureUnit::TerrainHeightmap());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainBiomeMap());
		glBindTexture(GL_TEXTURE_2D_ARRAY, biome_texture_);
		shader.trySetInt("uBiomeMap", Constants::TextureUnit::TerrainBiomeMap());
		shader.trySetInt("u_biomeMap", Constants::TextureUnit::TerrainBiomeMap());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainBakedParams());
		glBindTexture(GL_TEXTURE_2D_ARRAY, baked_params_texture_);
		shader.trySetInt("uBakedParams", Constants::TextureUnit::TerrainBakedParams());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainDisplacement());
		glBindTexture(GL_TEXTURE_2D_ARRAY, displacement_texture_);
		shader.trySetInt("u_displacementArray", Constants::TextureUnit::TerrainDisplacement());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::NoiseSimplex());
		glBindTexture(GL_TEXTURE_3D, noise_texture_);
		shader.setInt("u_noiseTexture", Constants::TextureUnit::NoiseSimplex());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::NoiseCurl());
		glBindTexture(GL_TEXTURE_3D, curl_texture_);
		shader.setInt("u_curlTexture", Constants::TextureUnit::NoiseCurl());

		if (extra_noise_texture_ != 0) {
			glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::NoiseExtra());
			glBindTexture(GL_TEXTURE_3D, extra_noise_texture_);
			shader.setInt("u_extraNoiseTexture", Constants::TextureUnit::NoiseExtra());
		}

		if (blue_noise_texture_ != 0) {
			glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::NoiseBlue());
			glBindTexture(GL_TEXTURE_2D, blue_noise_texture_);
			shader.trySetInt("u_blueNoiseTexture", Constants::TextureUnit::NoiseBlue());
		}

		if (phasor_noise_texture_ != 0) {
			glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::NoisePhasor());
			glBindTexture(GL_TEXTURE_2D, phasor_noise_texture_);
			shader.trySetInt("u_phasorTexture", Constants::TextureUnit::NoisePhasor());
		}

		// Bind Biome UBO
		glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::Biomes(), biome_ubo_);

		// Bind prebaked direct rendering VAO
		glBindVertexArray(terrain_vao_);

		// Wait for GPU to finish culling and building indirect draw commands
		int current_idx = chunk_states_pb_->GetCurrentBufferIndex();
		if (patch_fences_[current_idx]) {
			glClientWaitSync(patch_fences_[current_idx], GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
			glDeleteSync(patch_fences_[current_idx]);
			patch_fences_[current_idx] = 0;
		}

		IndirectCommandBuffer* mapped_indirect = terrain_indirect_pb_->GetFrameDataPtr();
		uint32_t count = mapped_indirect->draw_count;

		// Bind direct command buffer
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, terrain_indirect_pb_->GetBufferId());

		glMultiDrawElementsIndirect(
			GL_TRIANGLES,
			GL_UNSIGNED_INT,
			(void*)(uintptr_t)(terrain_indirect_pb_->GetFrameOffset() + offsetof(IndirectCommandBuffer, commands)),
			count,
			sizeof(DrawElementsIndirectCommand)
		);

		glBindVertexArray(0);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	}

	void TerrainRenderManager::CommitUpdates(bool force_sync) {
		// Ensure grid textures and UBO are up to date before baking
		UpdateGridTextures(last_world_scale_);
		PerformBaking(last_world_scale_, force_sync);
	}

	void TerrainRenderManager::PerformBaking(float world_scale, bool force_sync) {
		std::vector<BakeTask> tasks;
		{
			std::lock_guard<std::recursive_mutex> lock(mutex_);
			if (bake_queue_.empty())
				return;

			// Sort bake queue by distance to camera to prioritize nearby terrain
			std::sort(bake_queue_.begin(), bake_queue_.end(), [this, world_scale](const BakeTask& a, const BakeTask& b) {
				float scaled_chunk_size = chunk_size_ * world_scale;
				glm::vec2 pos_a = glm::vec2(a.chunk_coord) * scaled_chunk_size + scaled_chunk_size * 0.5f;
				glm::vec2 pos_b = glm::vec2(b.chunk_coord) * scaled_chunk_size + scaled_chunk_size * 0.5f;
				glm::vec2 cam_pos_2d = glm::vec2(last_camera_pos_.x, last_camera_pos_.z);
				return glm::distance2(pos_a, cam_pos_2d) < glm::distance2(pos_b, cam_pos_2d);
			});

			if (force_sync || bake_queue_.size() <= kMaxBakesPerFrame) {
				tasks = std::move(bake_queue_);
				bake_queue_.clear();
			} else {
				tasks.assign(bake_queue_.begin(), bake_queue_.begin() + kMaxBakesPerFrame);
				bake_queue_.erase(bake_queue_.begin(), bake_queue_.begin() + kMaxBakesPerFrame);
			}
		}

		if (world_scale <= 0.0f)
			world_scale = 1.0f; // Fallback for early calls

		if (!terrain_bake_shader_ || !terrain_bake_shader_->isValid())
			return;

		PROJECT_PROFILE_SCOPE("TerrainRenderManager::PerformBaking");

		terrain_bake_shader_->use();
		// Bindings are now handled via preprocessor tokens in the shader (layout(binding=...))

		// Bind raw input textures
		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainRawHeightmap());
		glBindTexture(GL_TEXTURE_2D_ARRAY, raw_heightmap_texture_);

		// Bind biome map as image for read/write status
		glBindImageTexture(Constants::TextureUnit::TerrainBiomeImage(), biome_texture_, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);

		// Bind output textures as images
		glBindImageTexture(Constants::TextureUnit::TerrainHeightmapImage(), heightmap_texture_, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
		glBindImageTexture(Constants::TextureUnit::TerrainBakedParamsImage(), baked_params_texture_, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
		glBindImageTexture(Constants::TextureUnit::TerrainDisplacementImage(), displacement_texture_, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);

		// Bind UBOs
		glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::TerrainData(), terrain_data_ubo_);
		if (visual_effects_ubo_ != 0) {
			glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::VisualEffects(), visual_effects_ubo_);
		}

		// Process in batches
		const size_t max_batch = 1024;
		for (size_t i = 0; i < tasks.size(); i += max_batch) {
			size_t batch_size = std::min(max_batch, tasks.size() - i);

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, bake_ssbo_);
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, batch_size * sizeof(BakeTask), &tasks[i]);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::TerrainChunkInfo(), bake_ssbo_);

			terrain_bake_shader_->setInt("u_numTasks", static_cast<int>(batch_size));

			// Local size is 8x8. Calculate workgroup counts to cover resolution.
			GLuint groups_x = (heightmap_resolution_ + 7) / 8;
			GLuint groups_y = (heightmap_resolution_ + 7) / 8;
			glDispatchCompute(groups_x, groups_y, static_cast<GLuint>(batch_size));
		}

		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

		// Dispatch patch metrics compute
		if (patch_metrics_shader_ && patch_metrics_shader_->isValid()) {
			PROJECT_PROFILE_SCOPE("TerrainRenderManager::DispatchPatchMetrics");
			patch_metrics_shader_->use();
			glBindImageTexture(Constants::TextureUnit::TerrainBiomeImage(), biome_texture_, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA8);
			glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainHeightmap());
			glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture_);
			patch_metrics_shader_->setInt("u_heightmapArray", Constants::TextureUnit::TerrainHeightmap());

			glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainBakedParams());
			glBindTexture(GL_TEXTURE_2D_ARRAY, baked_params_texture_);
			patch_metrics_shader_->setInt("u_bakedParamsArray", Constants::TextureUnit::TerrainBakedParams());

			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::TerrainPatchMetrics(), patch_metrics_ssbo_);
			glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::Biomes(), biome_ubo_);
			if (grass_props_ubo_ != 0) {
				glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::GrassProps(), grass_props_ubo_);
			}

			for (size_t i = 0; i < tasks.size(); i += max_batch) {
				size_t batch_size = std::min(max_batch, tasks.size() - i);
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, bake_ssbo_);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, batch_size * sizeof(BakeTask), &tasks[i]);
				glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::TerrainChunkInfo(), bake_ssbo_);
				patch_metrics_shader_->setInt("u_numTasks", static_cast<int>(batch_size));
				glDispatchCompute(1, 1, static_cast<GLuint>(batch_size));
			}
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		}

		// Update hierarchical mips for baked heightmaps
		if (height_mip_shader_ && height_mip_shader_->isValid()) {
			PROJECT_PROFILE_SCOPE("TerrainRenderManager::GenerateHeightMips");
			height_mip_shader_->use();
			int levels = 1 + static_cast<int>(std::floor(std::log2(heightmap_resolution_)));

			for (const auto& task : tasks) {
				height_mip_shader_->setInt("u_slice", task.slice);
				for (int mip = 1; mip < levels; ++mip) {
					int dst_w = std::max(1, heightmap_resolution_ >> mip);
					int dst_h = std::max(1, heightmap_resolution_ >> mip);

					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture_);
					height_mip_shader_->setInt("u_srcHeightmap", 0);
					height_mip_shader_->setInt("u_srcLevel", mip - 1);

					glBindImageTexture(0, heightmap_texture_, mip, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);

					glDispatchCompute((dst_w + 7) / 8, (dst_h + 7) / 8, 1);
					glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
				}
			}
		}

		// Update horizon map for these chunks
		UpdateHorizonMap(tasks);

		// Run our new mesh bake shader to generate prebaked vertices in the VBO
		if (terrain_mesh_bake_shader_ && terrain_mesh_bake_shader_->isValid() && !tasks.empty()) {
			PROJECT_PROFILE_SCOPE("TerrainRenderManager::BakeMeshVertices");
			terrain_mesh_bake_shader_->use();

			// Bind input textures (the already baked textures!)
			glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainHeightmap());
			glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture_);
			terrain_mesh_bake_shader_->setInt("u_heightmapArray", Constants::TextureUnit::TerrainHeightmap());

			glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainBakedParams());
			glBindTexture(GL_TEXTURE_2D_ARRAY, baked_params_texture_);
			terrain_mesh_bake_shader_->setInt("u_bakedParamsArray", Constants::TextureUnit::TerrainBakedParams());

			glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainDisplacement());
			glBindTexture(GL_TEXTURE_2D_ARRAY, displacement_texture_);
			terrain_mesh_bake_shader_->setInt("u_displacementArray", Constants::TextureUnit::TerrainDisplacement());

			glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainBiomeMap());
			glBindTexture(GL_TEXTURE_2D_ARRAY, biome_texture_);
			terrain_mesh_bake_shader_->setInt("u_biomeMap", Constants::TextureUnit::TerrainBiomeMap());

			// Bind UBOs
			glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::TerrainData(), terrain_data_ubo_);

			// Bind VBO pool as SSBO (reusing TerrainPatchDrawData binding 47)
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::TerrainPatchDrawData(), terrain_vbo_);

			for (const auto& task : tasks) {
				terrain_mesh_bake_shader_->setInt("u_slice", task.slice);
				terrain_mesh_bake_shader_->setInt("u_lod", task.target_lod);

				float scaled_chunk_size = chunk_size_ * world_scale;
				terrain_mesh_bake_shader_->setVec2("u_worldOffset", glm::vec2(task.chunk_coord) * scaled_chunk_size);

				// Compute dispatch size based on target LOD: grid width = (32 >> lod) + 1
				int gridWidth = (chunk_size_ >> task.target_lod) + 1;
				GLuint groups_x = (gridWidth + 15) / 16;
				GLuint groups_y = (gridWidth + 15) / 16;

				glDispatchCompute(groups_x, groups_y, 1);
			}
			glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
		}

		// Update chunk states on completion
		{
			std::lock_guard<std::recursive_mutex> lock(mutex_);
			for (const auto& task : tasks) {
				auto key = std::make_pair(task.chunk_coord.x, task.chunk_coord.y);
				auto it = chunks_.find(key);
				if (it != chunks_.end()) {
					it->second.current_lod = task.target_lod;
				}
			}
		}

		// Synchronize to ensure initial loads are fully baked before rendering
		if (force_sync) {
			glFinish();
		}
	}

	size_t TerrainRenderManager::GetRegisteredChunkCount() const {
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		return chunks_.size();
	}

	size_t TerrainRenderManager::GetVisibleChunkCount() const {
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		return visible_instances_.size();
	}

	std::vector<glm::vec4> TerrainRenderManager::GetChunkInfo(float world_scale) const {
		std::lock_guard<std::recursive_mutex> lock(mutex_);
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

	void TerrainRenderManager::UpdateHorizonMap(const std::vector<BakeTask>& tasks) {
		if (!terrain_horizon_shader_ || !terrain_horizon_shader_->isValid() || tasks.empty())
			return;

		PROJECT_PROFILE_SCOPE("TerrainRenderManager::UpdateHorizonMap");

		terrain_horizon_shader_->use();

		// Bind textures
		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainChunkGrid());
		glBindTexture(GL_TEXTURE_2D, chunk_grid_texture_);
		terrain_horizon_shader_->setInt("u_chunkGrid", Constants::TextureUnit::TerrainChunkGrid());


		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainHeightmap());
		glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture_);
		terrain_horizon_shader_->setInt("u_heightmapArray", Constants::TextureUnit::TerrainHeightmap());

		// Bind horizon map as image
		glBindImageTexture(Constants::TextureUnit::TerrainHorizonMap(), horizon_map_texture_, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);

		// Bind UBO
		glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::TerrainData(), terrain_data_ubo_);

		// Setup task SSBO (recycling bake_ssbo_ for simplicity)
		const size_t max_batch = 1024;
		for (size_t i = 0; i < tasks.size(); i += max_batch) {
			size_t batch_size = std::min(max_batch, tasks.size() - i);

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, bake_ssbo_);
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, batch_size * sizeof(BakeTask), &tasks[i]);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::TerrainChunkInfo(), bake_ssbo_);

			terrain_horizon_shader_->setInt("u_numTasks", static_cast<int>(batch_size));

			glDispatchCompute(1, 1, static_cast<GLuint>(batch_size));
		}

		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
	}

	void TerrainRenderManager::UpdateTerrainShadowMap(const glm::vec3& light_dir, float world_scale) {
		if (!terrain_shadow_map_shader_ || !terrain_shadow_map_shader_->isValid())
			return;

		// Only update if light moved or terrain changed or grid shifted
		bool grid_shifted = (last_grid_origin_x_ != last_shadow_grid_origin_x_ ||
		                     last_grid_origin_z_ != last_shadow_grid_origin_z_ ||
		                     world_scale != last_shadow_grid_world_scale_);

		float dot_diff = glm::dot(light_dir, last_shadow_light_dir_);

		if (dot_diff > 0.9999f && !grid_shifted && !grid_dirty_)
			return;

		PROJECT_PROFILE_SCOPE("TerrainRenderManager::UpdateTerrainShadowMap");

		terrain_shadow_map_shader_->use();

		// Bind all necessary terrain data
		BindTerrainData(*terrain_shadow_map_shader_);

		// Bind shadow map as image
		glBindImageTexture(Constants::TextureUnit::TerrainShadowMapImage(), terrain_shadow_map_texture_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8);

		// Bind horizon map as texture
		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainHorizonMap());
		glBindTexture(GL_TEXTURE_2D_ARRAY, horizon_map_texture_);
		terrain_shadow_map_shader_->setInt("u_terrainHorizonMap", Constants::TextureUnit::TerrainHorizonMap());

		terrain_shadow_map_shader_->setVec3("u_lightDir", light_dir);

		// Calculate shadow map area based on camera/grid
		float scaled_chunk_size = chunk_size_ * world_scale;
		float shadow_size = Constants::Class::Terrain::SliceMapSize() * scaled_chunk_size;
		glm::vec2 shadow_origin = glm::vec2(last_grid_origin_x_, last_grid_origin_z_) * scaled_chunk_size;

		terrain_shadow_map_shader_->setVec2("u_shadowOrigin", shadow_origin);
		terrain_shadow_map_shader_->setFloat("u_shadowSize", shadow_size);

		// Dispatch (8192x8192, local size 8x8)
		glDispatchCompute(1024, 1024, 1);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

		last_shadow_light_dir_ = light_dir;
		last_shadow_grid_origin_x_ = last_grid_origin_x_;
		last_shadow_grid_origin_z_ = last_grid_origin_z_;
		last_shadow_grid_world_scale_ = world_scale;
	}

	std::vector<TerrainRenderManager::DecorChunkData> TerrainRenderManager::GetDecorChunkData(float world_scale) const {
		std::lock_guard<std::recursive_mutex> lock(mutex_);
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
