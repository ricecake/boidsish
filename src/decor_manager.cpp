#include "decor_manager.h"

#include <algorithm>
#include <set>

#include "ConfigManager.h"
#include "geometry.h"
#include "graphics.h"
#include "logger.h"
#include "terrain_generator_interface.h"
#include "terrain_render_manager.h"
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>

namespace Boidsish {

	DecorManager::DecorManager() {}

	// Use DrawElementsIndirectCommand from geometry.h

	DecorManager::~DecorManager() {
		for (auto& type : decor_types_) {
			if (type.ssbo != 0)
				glDeleteBuffers(1, &type.ssbo);
			if (type.visible_ssbo != 0)
				glDeleteBuffers(1, &type.visible_ssbo);
			if (type.count_buffer != 0)
				glDeleteBuffers(1, &type.count_buffer);
			if (type.indirect_buffer != 0)
				glDeleteBuffers(1, &type.indirect_buffer);
			if (type.shadow_indirect_buffer != 0)
				glDeleteBuffers(1, &type.shadow_indirect_buffer);
		}
	}

	void DecorManager::_Initialize() {
		if (initialized_)
			return;

		placement_shader_ = std::make_unique<ComputeShader>("shaders/decor_placement.comp");
		culling_shader_ = std::make_unique<ComputeShader>("shaders/decor_cull.comp");
		update_commands_shader_ = std::make_unique<ComputeShader>("shaders/decor_update_commands.comp");

		// Check if compute shader compiled successfully
		if (!placement_shader_->isValid() || !culling_shader_->isValid() || !update_commands_shader_->isValid()) {
			logger::ERROR("Failed to compile decor compute shaders - decor will be disabled");
			initialized_ = true; // Mark as initialized to prevent repeated attempts
			return;
		}

		// Initialize free blocks
		free_blocks_.clear();
		for (int i = kMaxActiveChunks - 1; i >= 0; --i) {
			free_blocks_.push_back(i);
		}

		initialized_ = true;
	}

	void DecorManager::AddDecorType(const std::string& model_path, float density) {
		DecorProperties props;
		props.SetDensity(density);
		AddDecorType(model_path, props);
	}

	void DecorManager::AddDecorType(const std::string& model_path, const DecorProperties& props) {
		AddDecorType(std::make_shared<Model>(model_path), props);
	}

	void DecorManager::AddDecorType(std::shared_ptr<Model> model, float density) {
		DecorProperties props;
		props.SetDensity(density);
		AddDecorType(model, props);
	}

	void DecorManager::AddDecorType(std::shared_ptr<Model> model, const DecorProperties& props) {
		_Initialize();

		DecorType type;
		type.model = model;
		type.props = props;

		// Main instance storage (persistent)
		glGenBuffers(1, &type.ssbo);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, type.ssbo);
		glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxInstancesPerType * sizeof(glm::mat4), nullptr, GL_STATIC_DRAW);

		// Visible instances (filled per frame)
		glGenBuffers(1, &type.visible_ssbo);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, type.visible_ssbo);
		glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxInstancesPerType * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);

		// Indirect commands (one per mesh)
		const auto&                              meshes = type.model->getMeshes();
		size_t                                   num_meshes = meshes.size();
		std::vector<DrawElementsIndirectCommand> commands(num_meshes);
		for (size_t i = 0; i < num_meshes; ++i) {
			commands[i].count = static_cast<unsigned int>(meshes[i].indices.size());
			commands[i].instanceCount = 0; // Filled by GPU
			commands[i].firstIndex = 0;    // Standard mesh rendering
			commands[i].baseVertex = 0;
			commands[i].baseInstance = 0;
		}

		glGenBuffers(1, &type.indirect_buffer);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, type.indirect_buffer);
		glBufferData(
			GL_DRAW_INDIRECT_BUFFER,
			num_meshes * sizeof(DrawElementsIndirectCommand),
			commands.data(),
			GL_STATIC_DRAW
		);

		// Shadow indirect commands
		glGenBuffers(1, &type.shadow_indirect_buffer);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, type.shadow_indirect_buffer);
		glBufferData(
			GL_DRAW_INDIRECT_BUFFER,
			num_meshes * sizeof(DrawElementsIndirectCommand),
			commands.data(),
			GL_STATIC_DRAW
		);

		// Atomic counter for culling
		glGenBuffers(1, &type.count_buffer);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, type.count_buffer);
		glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(unsigned int), nullptr, GL_DYNAMIC_DRAW);

		decor_types_.push_back(type);
	}

	void DecorManager::AddProceduralDecor(ProceduralType type, const DecorProperties& props, int variants) {
		if (variants <= 0)
			return;

		float variant_min_density = props.min_density / variants;
		float variant_max_density = props.max_density / variants;

		for (int i = 0; i < variants; ++i) {
			auto            model = ProceduralGenerator::Generate(type, 1337 + i);
			DecorProperties variant_props = props;
			variant_props.min_density = variant_min_density;
			variant_props.max_density = variant_max_density;

			AddDecorType(model, variant_props);
		}
	}

	void DecorManager::PopulateDefaultDecor() {
		if (!decor_types_.empty())
			return;


		AddDecorType(
			"assets/decor/Apple tree/AppleTree.obj",
			{
				.min_density = 0.005f,
				.max_density = 0.01f,
				.base_scale = 0.5f,
				.scale_variance = 0.01f,
				.min_height = 5.0f,
				.max_height = 95.0f,
				.random_yaw = true,
				.biomes = {Biome::Forest, Biome::AlpineMeadow},
				.wind_responsiveness = 0.3f
			}
		);
		AddDecorType(
			"assets/decor/Tree/tree01.obj",
			{
				.min_density = 0.01f,
				.max_density = 0.02f,
				.base_scale = 0.015f,
				.scale_variance = 0.01f,
				.min_height = 5.0f,
				.max_height = 95.0f,
				.random_yaw = true,
				.biomes = {Biome::LushGrass, Biome::Forest},
				.wind_responsiveness = 0.5f
			}
		);

		AddDecorType(
			"assets/decor/Rose bush/Mesh_RoseBush.obj",
			{
				.min_density = 0.001f,
				.max_density = 0.005f,
				.base_scale = 0.05f,
				.scale_variance = 0.01f,
				.min_height = 5.0f,
				.max_height = 95.0f,
				.random_yaw = true,
				.align_to_terrain = true,
				.biomes = {Biome::LushGrass, Biome::AlpineMeadow},
				.wind_responsiveness = 0.25f
			}
		);
		AddDecorType(
			"assets/decor/Sunflower/PUSHILIN_sunflower.obj",
			{
				.min_density = 0.01f,
				.max_density = 0.05f,
				.base_scale = 0.5f,
				.scale_variance = 0.01f,
				.min_height = 5.0f,
				.max_height = 95.0f,
				.random_yaw = true,
				.biomes = {Biome::LushGrass, Biome::AlpineMeadow},
				.wind_responsiveness = 0.25f
			}
		);


		// // Procedural Flowers
		// AddProceduralDecor(
		// 	ProceduralType::Flower,
		// 	{
		// 		.min_density = 0.1f,
		// 		.max_density = 0.2f,
		// 		.base_scale = 0.5f,
		// 		.scale_variance = 0.1f,
		// 		.min_height = 5.0f,
		// 		.max_height = 100.0f,
		// 		.random_yaw = true,
		// 		.align_to_terrain = true,
		// 		.biomes = {Biome::LushGrass, Biome::AlpineMeadow}
		// 	},
		// 	4
		// );

		// Procedural Rocks
		// AddProceduralDecor(
		// 	ProceduralType::Rock,
		// 	{
		// 		.min_density = 0.02f,
		// 		.max_density = 0.04f,
		// 		.base_scale = 0.4f,
		// 		.scale_variance = 0.1f,
		// 		.min_height = 0.01f,
		// 		.max_height = 1000.0f,
		// 		.random_yaw = true,
		// 		.align_to_terrain = true,
		// 		.biomes = {Biome::BrownRock, Biome::GreyRock, Biome::DryGrass},
		// 		.wind_responsiveness = 0
		// 	},
		// 	2
		// );

		// Procedural Grass
		AddProceduralDecor(
			ProceduralType::Grass,
			{
				.min_density = 0.15f,
				.max_density = 0.25f,
				.base_scale = 0.5f,
				.scale_variance = 0.1f,
				.min_height = 0.01f,
				.max_height = 200.0f,
				.random_yaw = true,
				.align_to_terrain = true,
				.biomes = {Biome::LushGrass, Biome::DryGrass, Biome::Forest, Biome::AlpineMeadow},
				.wind_responsiveness = 1,
				.wind_rim_highlight = 1.0f
			},
			2
		);
	}

	DecorProperties DecorManager::GetDefaultTreeProperties() {
		DecorProperties props;
		props.min_height = 0.01f;
		props.max_height = 95.0f;
		props.min_density = 0.1f;
		props.max_density = 0.11f;
		props.base_scale = 0.008f;
		props.scale_variance = 0.01f;
		props.biomes = {Biome::LushGrass, Biome::Forest};
		props.wind_responsiveness = 1.0f;
		props.wind_rim_highlight = 0.15f;
		return props;
	}

	DecorProperties DecorManager::GetDefaultDeadTreeProperties() {
		DecorProperties props;
		props.min_height = 30.0f;
		props.max_height = 95.0f;
		props.min_density = 0.1f;
		props.max_density = 0.11f;
		props.base_scale = 0.8f;
		props.scale_variance = 0.01f;
		props.biomes = {Biome::DryGrass, Biome::AlpineMeadow};
		props.wind_responsiveness = 0.3f;
		props.wind_rim_highlight = 0.05f;
		return props;
	}

	DecorProperties DecorManager::GetDefaultRockProperties() {
		DecorProperties props;
		props.max_density = 1.5f;
		props.base_scale = 0.002f;
		props.scale_variance = 0.1f;
		props.biomes = {Biome::BrownRock, Biome::GreyRock};
		props.align_to_terrain = true;
		props.wind_responsiveness = 0.0f;
		props.wind_rim_highlight = 0.0f;
		return props;
	}

	void DecorManager::PrepareResources(Megabuffer* mb) {
		if (!mb)
			return;

		for (auto& type : decor_types_) {
			type.model->PrepareResources(mb);

			// Update indirect buffers with correct Megabuffer offsets
			const auto&                              meshes = type.model->getMeshes();
			size_t                                   num_meshes = meshes.size();
			std::vector<DrawElementsIndirectCommand> commands(num_meshes);
			std::vector<DrawElementsIndirectCommand> shadow_commands(num_meshes);

			for (size_t i = 0; i < num_meshes; ++i) {
				const auto& mesh = meshes[i];
				// Regular commands
				commands[i].count = mesh.allocation.valid ? mesh.allocation.index_count
														  : static_cast<uint32_t>(mesh.indices.size());
				commands[i].instanceCount = 0;
				commands[i].firstIndex = mesh.allocation.valid ? mesh.allocation.first_index : 0;
				commands[i].baseVertex = mesh.allocation.valid ? mesh.allocation.base_vertex : 0;
				commands[i].baseInstance = 0;

				// Shadow commands
				if (mesh.shadow_allocation.valid) {
					shadow_commands[i].count = mesh.shadow_allocation.index_count;
					shadow_commands[i].firstIndex = mesh.shadow_allocation.first_index;
				} else if (!mesh.shadow_indices.empty()) {
					shadow_commands[i].count = static_cast<uint32_t>(mesh.shadow_indices.size());
					shadow_commands[i].firstIndex = static_cast<uint32_t>(mesh.indices.size());
				} else {
					shadow_commands[i].count = commands[i].count;
					shadow_commands[i].firstIndex = commands[i].firstIndex;
				}
				shadow_commands[i].instanceCount = 0;
				shadow_commands[i].baseVertex = commands[i].baseVertex;
				shadow_commands[i].baseInstance = 0;
			}

			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, type.indirect_buffer);
			glBufferSubData(
				GL_DRAW_INDIRECT_BUFFER,
				0,
				num_meshes * sizeof(DrawElementsIndirectCommand),
				commands.data()
			);

			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, type.shadow_indirect_buffer);
			glBufferSubData(
				GL_DRAW_INDIRECT_BUFFER,
				0,
				num_meshes * sizeof(DrawElementsIndirectCommand),
				shadow_commands.data()
			);
		}
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
	}

	void DecorManager::Update(
		float                                 delta_time,
		const Camera&                         camera,
		const Frustum&                        frustum,
		const ITerrainGenerator&              terrain_gen,
		std::shared_ptr<TerrainRenderManager> render_manager
	) {
		if (!enabled_ || !initialized_ || decor_types_.empty())
			return;

		if (!placement_shader_ || !placement_shader_->isValid() || !culling_shader_ || !culling_shader_->isValid())
			return;

		if (!render_manager)
			return;

		_UpdateAllocation(camera, frustum, terrain_gen, render_manager);
	}

	void DecorManager::_UpdateAllocation(
		const Camera&                         camera,
		const Frustum&                        frustum,
		const ITerrainGenerator&              terrain_gen,
		std::shared_ptr<TerrainRenderManager> render_manager
	) {
		float world_scale = terrain_gen.GetWorldScale();

		// Get the heightmap and biome texture arrays from the terrain render manager
		GLuint heightmap_texture = render_manager->GetHeightmapTexture();
		GLuint biome_texture = render_manager->GetBiomeTexture();
		if (heightmap_texture == 0 || biome_texture == 0)
			return;

		auto chunk_info = render_manager->GetChunkInfo(world_scale);
		if (chunk_info.empty())
			return;

		uint32_t current_terrain_version = terrain_gen.GetVersion();

		// 1. Identify chunks that should be active (within range and visible/preload)
		std::vector<std::pair<float, std::pair<int, int>>> chunks_to_keep;
		const float                                        kPreloadRadius = 128.0f * world_scale;

		for (const auto& chunk : chunk_info) {
			glm::vec2 chunk_offset(chunk.x, chunk.y);
			float     chunk_size = chunk.w;
			glm::vec2 chunk_center_2d = chunk_offset + glm::vec2(chunk_size * 0.5f);

			float dist = glm::distance(glm::vec2(camera.x, camera.z), chunk_center_2d);
			if (dist > max_decor_distance_ * world_scale)
				continue;

			// Approximate AABB for frustum culling
			glm::vec3 chunk_min(chunk_offset.x, -100.0f * world_scale, chunk_offset.y);
			glm::vec3 chunk_max(
				chunk_offset.x + chunk_size,
				terrain_gen.GetMaxHeight() + 100.0f * world_scale,
				chunk_offset.y + chunk_size
			);

			bool in_preload = dist < kPreloadRadius;
			bool in_frustum = frustum.IsBoxInFrustum(chunk_min, chunk_max);

			if (!in_preload && !in_frustum)
				continue;

			// Store key and distance (for priority)
			int cx = static_cast<int>(std::floor(chunk.x / chunk.w + 0.5f));
			int cz = static_cast<int>(std::floor(chunk.y / chunk.w + 0.5f));
			chunks_to_keep.push_back({dist, {cx, cz}});
		}

		// 2. Cull chunks that are no longer in range or visible
		std::vector<std::pair<int, int>> active_keys;
		for (const auto& ck : chunks_to_keep)
			active_keys.push_back(ck.second);

		std::vector<glm::mat4> zeros; // Reused for zeroing out blocks

		for (auto it = active_chunks_.begin(); it != active_chunks_.end();) {
			auto key = it->first;
			bool should_keep = std::find(active_keys.begin(), active_keys.end(), key) != active_keys.end();

			if (!should_keep) {
				int block = it->second.block_index;
				free_blocks_.push_back(block);

				if (zeros.empty()) {
					zeros.assign(kInstancesPerChunk, glm::mat4(0.0f));
				}

				// Zero out the block in all SSBOs to ensure it doesn't render
				for (auto& type : decor_types_) {
					glBindBuffer(GL_SHADER_STORAGE_BUFFER, type.ssbo);
					glBufferSubData(
						GL_SHADER_STORAGE_BUFFER,
						block * kInstancesPerChunk * sizeof(glm::mat4),
						zeros.size() * sizeof(glm::mat4),
						zeros.data()
					);
				}

				it = active_chunks_.erase(it);
			} else {
				++it;
			}
		}

		// 3. Allocate blocks for new chunks
		std::sort(chunks_to_keep.begin(), chunks_to_keep.end()); // Closer first

		std::vector<std::pair<std::pair<int, int>, int>> chunks_to_generate;

		for (const auto& vc : chunks_to_keep) {
			auto key = vc.second;
			if (active_chunks_.find(key) == active_chunks_.end()) {
				if (!free_blocks_.empty()) {
					int block = free_blocks_.back();
					free_blocks_.pop_back();
					active_chunks_[key] = {block, current_terrain_version, true};
					chunks_to_generate.push_back({key, block});
				}
			} else if (active_chunks_[key].terrain_version != current_terrain_version) {
				active_chunks_[key].terrain_version = current_terrain_version;
				active_chunks_[key].is_dirty = true;
				chunks_to_generate.push_back({key, active_chunks_[key].block_index});
			}
		}

		// 4. Dispatch placement compute for new/dirty chunks
		if (!chunks_to_generate.empty()) {
			placement_shader_->use();
			placement_shader_->setVec2("u_cameraPos", glm::vec2(camera.x, camera.z));
			placement_shader_->setFloat("u_maxTerrainHeight", terrain_gen.GetMaxHeight());
			placement_shader_->setInt("u_maxInstances", kMaxInstancesPerType);
			placement_shader_->setFloat("u_worldScale", world_scale);
			placement_shader_->setFloat("u_densityFalloffStart", density_falloff_start_ * world_scale);
			placement_shader_->setFloat("u_densityFalloffEnd", density_falloff_end_ * world_scale);
			placement_shader_->setFloat("u_maxDecorDistance", max_decor_distance_ * world_scale);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture);
			placement_shader_->setInt("u_heightmapArray", 0);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D_ARRAY, biome_texture);
			placement_shader_->setInt("u_biomeMap", 1);

			for (size_t i = 0; i < decor_types_.size(); ++i) {
				auto& type = decor_types_[i];
				placement_shader_->setFloat("u_minDensity", type.props.min_density);
				placement_shader_->setFloat("u_maxDensity", type.props.max_density);
				placement_shader_->setFloat("u_baseScale", type.props.base_scale);
				placement_shader_->setFloat("u_scaleVariance", type.props.scale_variance);
				placement_shader_->setFloat("u_minHeight", type.props.min_height);
				placement_shader_->setFloat("u_maxHeight", type.props.max_height);
				placement_shader_->setFloat("u_minSlope", type.props.min_slope);
				placement_shader_->setFloat("u_maxSlope", type.props.max_slope);
				placement_shader_->setVec3("u_baseRotation", glm::radians(type.props.base_rotation));
				placement_shader_->setBool("u_randomYaw", type.props.random_yaw);
				placement_shader_->setBool("u_alignToTerrain", type.props.align_to_terrain);
				placement_shader_->setUint("u_biomeMask", (uint32_t)type.props.biomes);
				placement_shader_->setFloat("u_detailDistance", type.props.detail_distance);
				placement_shader_->setInt("u_typeIndex", (int)i);
				placement_shader_->setVec3("u_aabbMin", type.model->GetAABB().min);
				placement_shader_->setVec3("u_aabbMax", type.model->GetAABB().max);

				glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, type.ssbo);

				for (const auto& [key, block] : chunks_to_generate) {
					// Find the chunk info for this key
					for (const auto& chunk : chunk_info) {
						int cx = static_cast<int>(std::floor(chunk.x / chunk.w + 0.5f));
						int cz = static_cast<int>(std::floor(chunk.y / chunk.w + 0.5f));
						if (cx == key.first && cz == key.second) {
							placement_shader_->setVec2("u_chunkWorldOffset", glm::vec2(chunk.x, chunk.y));
							placement_shader_->setFloat("u_chunkSlice", chunk.z);
							placement_shader_->setFloat("u_chunkSize", chunk.w);
							placement_shader_->setInt("u_baseInstanceIndex", block * kInstancesPerChunk);

							glDispatchCompute(4, 4, 1); // 32x32 = 1024 threads
							break;
						}
					}
				}
			}
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		}
	}

	void DecorManager::Render(
		const glm::mat4&                view,
		const glm::mat4&                projection,
		int                             viewport_width,
		int                             viewport_height,
		const std::optional<glm::mat4>& light_space_matrix,
		Shader*                         shader_override
	) {
		if (!enabled_ || !initialized_ || decor_types_.empty())
			return;

		bool is_shadow_pass = light_space_matrix.has_value();

		// 1. GPU Culling Pass
		// Use light space matrix directly for shadow pass culling
		Frustum   frustum = is_shadow_pass ? Frustum::FromViewProjection(glm::mat4(1.0f), *light_space_matrix)
                                         : Frustum::FromViewProjection(view, projection);
		glm::mat4 viewProj = is_shadow_pass ? *light_space_matrix : projection * view;

		culling_shader_->use();
		for (int p = 0; p < 6; ++p) {
			culling_shader_->setVec4(
				"u_frustumPlanes[" + std::to_string(p) + "]",
				glm::vec4(frustum.planes[p].normal, frustum.planes[p].distance)
			);
		}
		culling_shader_->setInt("u_totalSlots", kMaxInstancesPerType);
		culling_shader_->setMat4("u_viewProj", viewProj);
		culling_shader_->setVec2("u_viewportSize", glm::vec2((float)viewport_width, (float)viewport_height));
		culling_shader_->setFloat("u_minPixelSize", min_pixel_size_);

		// Hi-Z occlusion culling uniforms
		culling_shader_->setBool("u_enableHiZ", hiz_enabled_ && !is_shadow_pass);
		if (hiz_enabled_ && !is_shadow_pass) {
			glActiveTexture(GL_TEXTURE15);
			glBindTexture(GL_TEXTURE_2D, hiz_texture_);
			culling_shader_->setInt("u_hizTexture", 15);
			culling_shader_->setMat4("u_prevViewProjection", hiz_prev_vp_);
			glUniform2i(glGetUniformLocation(culling_shader_->ID, "u_hizSize"), hiz_width_, hiz_height_);
			culling_shader_->setInt("u_hizMipCount", hiz_mip_count_);
		}

		for (auto& type : decor_types_) {
			// Reset atomic counter
			unsigned int zero = 0;
			glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, type.count_buffer);
			glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(unsigned int), &zero);

			// Cull instances
			culling_shader_->use();
			culling_shader_->setVec3("u_aabbMin", type.model->GetAABB().min);
			culling_shader_->setVec3("u_aabbMax", type.model->GetAABB().max);

			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, type.ssbo);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, type.visible_ssbo);
			glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, type.count_buffer);
			glDispatchCompute(kMaxInstancesPerType / 64, 1, 1);

			glMemoryBarrier(GL_ATOMIC_COUNTER_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

			// Update indirect commands for both regular and shadow passes
			update_commands_shader_->use();
			update_commands_shader_->setInt("u_numCommands", (int)type.model->getMeshes().size());
			glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, type.count_buffer);

			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, type.indirect_buffer);
			glDispatchCompute(1, 1, 1);

			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, type.shadow_indirect_buffer);
			glDispatchCompute(1, 1, 1);

			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
		}

		// 2. Rendering Pass
		Shader* shader = shader_override ? shader_override : Shape::shader.get();
		if (!shader)
			return;

		shader->use();
		shader->setMat4("view", view);
		shader->setMat4("projection", projection);
		if (is_shadow_pass) {
			shader->setMat4("lightSpaceMatrix", *light_space_matrix);
		}
		shader->setMat4("model", glm::mat4(1.0f));
		shader->setBool("useSSBOInstancing", true);
		shader->setBool("isTextEffect", false);
		shader->setBool("isColossal", false);
		shader->setBool("is_instanced", false);
		shader->setVec3("objectColor", 1.0f, 1.0f, 1.0f);
		shader->setBool("usePBR", false);
		shader->setVec4("clipPlane", 0.0f, 0.0f, 0.0f, 0.0f);
		shader->setFloat(
			"ripple_strength",
			ConfigManager::GetInstance().GetAppSettingBool("artistic_effect_ripple", false) ? 0.05f : 0.0f
		);

		for (size_t i = 0; i < decor_types_.size(); ++i) {
			auto& type = decor_types_[i];

			// Bind the culled instances SSBO
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, type.visible_ssbo);

			if (type.model->IsNoCull()) {
				glDisable(GL_CULL_FACE);
			}

			shader->setVec3("u_aabbMin", type.model->GetAABB().min);
			shader->setVec3("u_aabbMax", type.model->GetAABB().max);
			shader->setFloat("u_windResponsiveness", type.props.wind_responsiveness);
			shader->setFloat("u_windRimHighlight", type.props.wind_rim_highlight);

			// Bind the appropriate indirect buffer
			if (is_shadow_pass) {
				glBindBuffer(GL_DRAW_INDIRECT_BUFFER, type.shadow_indirect_buffer);
			} else {
				glBindBuffer(GL_DRAW_INDIRECT_BUFFER, type.indirect_buffer);
			}

			const auto& meshes = type.model->getMeshes();
			for (size_t mi = 0; mi < meshes.size(); ++mi) {
				const auto& mesh = meshes[mi];
				bool        hasDiffuse = false;
				for (const auto& t : mesh.textures) {
					if (t.type == "texture_diffuse") {
						hasDiffuse = true;
						break;
					}
				}
				shader->setBool("use_texture", hasDiffuse);
				shader->setBool("useVertexColor", true); // Enable vertex color for decor
				mesh.bindTextures(*shader);

				glBindVertexArray(mesh.getVAO());
				glDrawElementsIndirect(
					GL_TRIANGLES,
					GL_UNSIGNED_INT,
					(void*)(mi * sizeof(DrawElementsIndirectCommand))
				);
			}

			if (type.model->IsNoCull()) {
				glEnable(GL_CULL_FACE);
			}
		}

		glBindVertexArray(0);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
		shader->setBool("useSSBOInstancing", false);
	}

} // namespace Boidsish
