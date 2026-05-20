#include "decor_manager.h"

#include <algorithm>

#include "service_locator.h"
#include <set>

#include "ConfigManager.h"
#include "NoiseManager.h"
#include "atmosphere_manager.h"
#include "geometry.h"
#include "graphics.h"
#include "logger.h"
#include "profiler.h"
#include "terrain_generator_interface.h"
#include "terrain_render_manager.h"
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>

namespace Boidsish {

	DecorManager::DecorManager(ServiceLocator& /*loc*/) {}

	DecorManager::~DecorManager() {
		for (auto& type : decor_types_) {
			if (type.ssbo != 0)
				glDeleteBuffers(1, &type.ssbo);
			if (type.visible_ssbo != 0)
				glDeleteBuffers(1, &type.visible_ssbo);
		}
		block_validity_pb_.reset();
		if (decor_props_ubo_ != 0)
			glDeleteBuffers(1, &decor_props_ubo_);
		if (placement_globals_ubo_ != 0)
			glDeleteBuffers(1, &placement_globals_ubo_);
		if (chunk_params_ssbo_ != 0)
			glDeleteBuffers(1, &chunk_params_ssbo_);
	}

	void DecorManager::_Initialize() {
		if (initialized_)
			return;

		placement_shader_ = std::make_unique<ComputeShader>("shaders/decor_placement.comp");
		culling_shader_ = std::make_unique<ComputeShader>("shaders/decor_cull.comp");
		update_commands_shader_ = std::make_unique<ComputeShader>("shaders/decor_update_commands.comp");

		if (!placement_shader_->isValid() || !culling_shader_->isValid() || !update_commands_shader_->isValid()) {
			logger::ERROR("Failed to compile decor compute shaders - decor will be disabled");
			initialized_ = true;
			return;
		}

		free_blocks_.clear();
		for (int i = kMaxActiveChunks - 1; i >= 0; --i) {
			free_blocks_.push_back(i);
		}

		block_validity_pb_ = std::make_unique<PersistentBuffer<uint32_t>>(GL_SHADER_STORAGE_BUFFER, kMaxActiveChunks, 3);
		std::memset(block_validity_pb_->GetFullBufferPtr(), 0, block_validity_pb_->GetTotalSize());

		glGenBuffers(1, &placement_globals_ubo_);
		glBindBuffer(GL_UNIFORM_BUFFER, placement_globals_ubo_);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(PlacementGlobalsGPU), nullptr, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		glGenBuffers(1, &chunk_params_ssbo_);

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

		glGenBuffers(1, &type.ssbo);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, type.ssbo);
		glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxInstancesPerType * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);

		glGenBuffers(1, &type.visible_ssbo);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, type.visible_ssbo);
		glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxInstancesPerType * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);

		const auto& meshes = type.model->getMeshes();
		size_t      num_meshes = meshes.size();

		type.indirect_pb = std::make_unique<PersistentBuffer<uint8_t>>(
			GL_DRAW_INDIRECT_BUFFER,
			num_meshes * sizeof(DrawElementsIndirectCommand),
			3
		);
		type.shadow_indirect_pb = std::make_unique<PersistentBuffer<uint8_t>>(
			GL_DRAW_INDIRECT_BUFFER,
			num_meshes * sizeof(DrawElementsIndirectCommand),
			3
		);
		type.count_pb = std::make_unique<PersistentBuffer<uint32_t>>(GL_ATOMIC_COUNTER_BUFFER, 1, 3);

		decor_types_.push_back(std::move(type));
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
			{.min_density = 0.005f,
		     .max_density = 0.01f,
		     .base_scale = 0.5f,
		     .scale_variance = 0.01f,
		     .min_height = 5.0f,
		     .max_height = 95.0f,
		     .random_yaw = true,
		     .biomes = {Biome::Forest, Biome::AlpineMeadow},
		     .wind_responsiveness = 0.3f}
		);
		AddDecorType(
			"assets/decor/Tree/tree01.obj",
			{.min_density = 0.001f,
		     .max_density = 0.005f,
		     .base_scale = 0.055f,
		     .scale_variance = 0.05f,
		     .min_height = 5.0f,
		     .max_height = 95.0f,
		     .random_yaw = true,
		     .biomes = {Biome::LushGrass, Biome::Forest},
		     .wind_responsiveness = 0.5f}
		);

		AddDecorType(
			"assets/decor/Rose bush/Mesh_RoseBush.obj",
			{.min_density = 0.001f,
		     .max_density = 0.005f,
		     .base_scale = 0.08f,
		     .scale_variance = 0.01f,
		     .min_height = 5.0f,
		     .max_height = 95.0f,
		     .random_yaw = true,
		     .align_to_terrain = true,
		     .biomes = {Biome::LushGrass, Biome::AlpineMeadow},
		     .wind_responsiveness = 0.25f}
		);
		AddDecorType(
			"assets/decor/Sunflower/PUSHILIN_sunflower.obj",
			{.min_density = 0.01f,
		     .max_density = 0.05f,
		     .base_scale = 0.5f,
		     .scale_variance = 0.01f,
		     .min_height = 5.0f,
		     .max_height = 95.0f,
		     .random_yaw = true,
		     .biomes = {Biome::LushGrass, Biome::AlpineMeadow},
		     .wind_responsiveness = 0.25f}
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
		for (auto& type : decor_types_) {
			if (mb) {
				type.model->PrepareResources(mb);
			}

			const auto& meshes = type.model->getMeshes();
			size_t      num_meshes = meshes.size();

			for (int f = 0; f < 3; ++f) {
				DrawElementsIndirectCommand* cmd_ptr =
					reinterpret_cast<DrawElementsIndirectCommand*>(type.indirect_pb->GetFrameDataPtr(f));
				DrawElementsIndirectCommand* shadow_cmd_ptr =
					reinterpret_cast<DrawElementsIndirectCommand*>(type.shadow_indirect_pb->GetFrameDataPtr(f));

				for (size_t i = 0; i < num_meshes; ++i) {
					const auto& mesh = meshes[i];
					cmd_ptr[i].count = mesh.allocation.valid ? mesh.allocation.index_count
															 : static_cast<uint32_t>(mesh.indices.size());
					cmd_ptr[i].instanceCount = 0;
					cmd_ptr[i].firstIndex = mesh.allocation.valid ? mesh.allocation.first_index : 0;
					cmd_ptr[i].baseVertex = mesh.allocation.valid ? mesh.allocation.base_vertex : 0;
					cmd_ptr[i].baseInstance = 0;

					if (mesh.shadow_allocation.valid) {
						shadow_cmd_ptr[i].count = mesh.shadow_allocation.index_count;
						shadow_cmd_ptr[i].firstIndex = mesh.shadow_allocation.first_index;
					} else if (!mesh.shadow_indices.empty()) {
						shadow_cmd_ptr[i].count = static_cast<uint32_t>(mesh.shadow_indices.size());
						shadow_cmd_ptr[i].firstIndex = static_cast<uint32_t>(mesh.indices.size());
					} else {
						shadow_cmd_ptr[i].count = cmd_ptr[i].count;
						shadow_cmd_ptr[i].firstIndex = cmd_ptr[i].firstIndex;
					}
					shadow_cmd_ptr[i].instanceCount = 0;
					shadow_cmd_ptr[i].baseVertex = cmd_ptr[i].baseVertex;
					shadow_cmd_ptr[i].baseInstance = 0;
				}
			}
		}

		if (decor_types_.empty())
			return;

		static constexpr int      kMaxDecorTypes = 32;
		std::vector<DecorTypeGPU> gpu_types(decor_types_.size());
		for (size_t i = 0; i < decor_types_.size(); ++i) {
			auto& t = decor_types_[i];
			auto& g = gpu_types[i];
			g.density_scale =
				glm::vec4(t.props.min_density, t.props.max_density, t.props.base_scale, t.props.scale_variance);
			g.height_slope = glm::vec4(t.props.min_height, t.props.max_height, t.props.min_slope, t.props.max_slope);
			g.rotation = glm::vec4(glm::radians(t.props.base_rotation), t.props.detail_distance);
			g.aabb_min = glm::vec4(t.model->GetData()->aabb.min, 0.0f);
			g.aabb_max = glm::vec4(t.model->GetData()->aabb.max, 0.0f);
			g.flags = glm::uvec4(
				static_cast<uint32_t>(t.props.biomes),
				t.props.random_yaw ? 1u : 0u,
				t.props.align_to_terrain ? 1u : 0u,
				static_cast<uint32_t>(i)
			);
		}

		if (decor_props_ubo_ == 0) {
			glGenBuffers(1, &decor_props_ubo_);
		}
		glBindBuffer(GL_UNIFORM_BUFFER, decor_props_ubo_);
		glBufferData(GL_UNIFORM_BUFFER, kMaxDecorTypes * sizeof(DecorTypeGPU), nullptr, GL_DYNAMIC_DRAW);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, gpu_types.size() * sizeof(DecorTypeGPU), gpu_types.data());
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	void DecorManager::Update(
		float                                 delta_time,
		const Camera&                         camera,
		const Frustum&                        frustum,
		const ITerrainGenerator&              terrain_gen,
		std::shared_ptr<TerrainRenderManager> render_manager
	) {
		PROJECT_PROFILE_SCOPE("DecorManager::Update");
		if (!enabled_ || !initialized_ || decor_types_.empty())
			return;

		if (!placement_shader_ || !placement_shader_->isValid() || !culling_shader_ || !culling_shader_->isValid())
			return;

		if (!render_manager)
			return;

		UpdateResult res = PrepareUpdate(camera, terrain_gen, render_manager);
		ApplyUpdate(res, render_manager);
	}

	DecorManager::UpdateResult DecorManager::PrepareUpdate(
		const Camera&                         camera,
		const ITerrainGenerator&              terrain_gen,
		std::shared_ptr<TerrainRenderManager> render_manager
	) {
		PROJECT_PROFILE_SCOPE("DecorManager::PrepareUpdate");
		UpdateResult res;

		frame_counter_++;
		camera_pos_ = camera.pos();
		glm::vec3 fwd = camera.front();
		glm::vec2 new_forward = glm::normalize(glm::vec2(fwd.x, fwd.z));
		camera_rotation_delta_ = new_forward - prev_camera_forward_2d_;
		prev_camera_forward_2d_ = camera_forward_2d_;
		camera_forward_2d_ = new_forward;

		float world_scale = terrain_gen.GetWorldScale();

		auto all_chunks = render_manager->GetDecorChunkData(world_scale);
		if (all_chunks.empty())
			return res;

		glm::vec2 cam_xz(camera.x, camera.z);
		float     max_dist = max_decor_distance_ * world_scale;

		struct CandidateChunk {
			int   src_index;
			float priority;
		};

		std::vector<CandidateChunk> candidates;
		candidates.reserve(all_chunks.size());

		float     rotation_magnitude = glm::length(camera_rotation_delta_);
		glm::vec2 rotation_dir = (rotation_magnitude > 0.001f) ? camera_rotation_delta_ / rotation_magnitude
															   : glm::vec2(0.0f);

		for (int i = 0; i < (int)all_chunks.size(); ++i) {
			const auto& cd = all_chunks[i];
			glm::vec2   center = cd.world_offset + glm::vec2(cd.chunk_size * 0.5f);
			float       dist = glm::distance(cam_xz, center);
			if (dist > max_dist)
				continue;

			glm::vec2 to_chunk = (dist > 0.1f) ? glm::normalize(center - cam_xz) : camera_forward_2d_;
			float     facing = glm::dot(camera_forward_2d_, to_chunk);

			float facing_weight;
			if (facing > 0.0f) {
				facing_weight = 0.3f + 1.7f * (1.0f - facing);
			} else {
				facing_weight = 2.0f + 3.0f * (-facing);
			}

			if (rotation_magnitude > 0.001f) {
				float turn_alignment = glm::dot(rotation_dir, to_chunk);
				float turn_boost = turn_alignment * std::min(rotation_magnitude * 30.0f, 1.5f);
				facing_weight -= turn_boost;
				facing_weight = std::max(facing_weight, 0.1f);
			}

			candidates.push_back({i, dist * facing_weight});
		}

		std::vector<std::pair<int, int>> active_keys;
		active_keys.reserve(candidates.size());
		for (const auto& c : candidates)
			active_keys.push_back(all_chunks[c.src_index].key);
		std::sort(active_keys.begin(), active_keys.end());

		for (auto it = active_chunks_.begin(); it != active_chunks_.end();) {
			bool is_present = std::binary_search(active_keys.begin(), active_keys.end(), it->first);

			if (is_present) {
				it->second.last_seen_frame = frame_counter_;
				++it;
			} else if (frame_counter_ - it->second.last_seen_frame > kChunkGracePeriodFrames) {
				int block = it->second.block_index;
				free_blocks_.push_back(block);
				res.free_blocks_to_invalidate.push_back(block);
				it = active_chunks_.erase(it);
			} else {
				++it;
			}
		}

		std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
			return a.priority < b.priority;
		});

		int chunks_new = 0;

		for (const auto& cand : candidates) {
			const auto& cd = all_chunks[cand.src_index];
			auto        it = active_chunks_.find(cd.key);

			if (it == active_chunks_.end()) {
				if (chunks_new >= kMaxNewChunksPerFrame)
					continue;
				if (free_blocks_.empty())
					continue;

				int block = free_blocks_.back();
				free_blocks_.pop_back();
				active_chunks_[cd.key] = {block, cd.update_count, true, frame_counter_};
				res.chunks_to_generate.push_back({cand.src_index, block});
				chunks_new++;
			} else if (it->second.update_count != cd.update_count) {
				it->second.update_count = cd.update_count;
				it->second.is_dirty = true;
				res.chunks_to_generate.push_back({cand.src_index, it->second.block_index});
			}
		}

		if (!res.chunks_to_generate.empty()) {
			int num_chunks = (int)res.chunks_to_generate.size();
			res.globals.camera_and_scale = glm::vec4(cam_xz, world_scale, terrain_gen.GetMaxHeight());
			res.globals.distance_params = glm::vec4(
				density_falloff_start_ * world_scale,
				density_falloff_end_ * world_scale,
				max_decor_distance_ * world_scale,
				static_cast<float>(kMaxInstancesPerType)
			);

			res.chunk_params.resize(num_chunks);
			for (int j = 0; j < num_chunks; ++j) {
				const auto& cd = all_chunks[res.chunks_to_generate[j].src_index];
				res.chunk_params[j].offset_slice_size = glm::vec4(cd.world_offset, cd.slice, cd.chunk_size);
				res.chunk_params[j].indices = glm::ivec4(res.chunks_to_generate[j].block * kInstancesPerChunk, 0, 0, 0);
			}
			res.valid = true;
		}

		return res;
	}

	void DecorManager::ApplyUpdate(const UpdateResult& res, std::shared_ptr<TerrainRenderManager> render_manager) {
		PROJECT_PROFILE_SCOPE("DecorManager::ApplyUpdate");
		if (!res.valid && res.free_blocks_to_invalidate.empty())
			return;

		for (int block : res.free_blocks_to_invalidate) {
			for (int f = 0; f < 3; ++f) {
				block_validity_pb_->GetFrameDataPtr(f)[block] = 0;
			}
		}

		if (res.valid) {
			int num_chunks = (int)res.chunks_to_generate.size();

			block_validity_pb_->AdvanceFrame();
			uint32_t* validity_ptr = block_validity_pb_->GetFrameDataPtr();
			int       prev_idx = (block_validity_pb_->GetCurrentBufferIndex() + 2) % 3;
			std::memcpy(validity_ptr, block_validity_pb_->GetFrameDataPtr(prev_idx), kMaxActiveChunks * sizeof(uint32_t));

			for (const auto& entry : res.chunks_to_generate) {
				validity_ptr[entry.block] = 1;
			}

			glBindBuffer(GL_UNIFORM_BUFFER, placement_globals_ubo_);
			glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(PlacementGlobalsGPU), &res.globals);

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunk_params_ssbo_);
			glBufferData(
				GL_SHADER_STORAGE_BUFFER,
				num_chunks * sizeof(ChunkParamsGPU),
				res.chunk_params.data(),
				GL_STREAM_DRAW
			);

			placement_shader_->use();

			glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainHeightmap());
			glBindTexture(GL_TEXTURE_2D_ARRAY, render_manager->GetHeightmapTexture());
			placement_shader_->setInt("u_heightmapArray", Constants::TextureUnit::TerrainHeightmap());

			glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainBiomeMap());
			glBindTexture(GL_TEXTURE_2D_ARRAY, render_manager->GetBiomeTexture());
			placement_shader_->setInt("u_biomeMap", Constants::TextureUnit::TerrainBiomeMap());

			glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::DecorProps(), decor_props_ubo_);
			glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::DecorPlacementGlobals(), placement_globals_ubo_);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::DecorChunkParams(), chunk_params_ssbo_);

			int dispatch_size = (Constants::Class::Terrain::ChunkSize() + 7) / 8;
			for (size_t i = 0; i < decor_types_.size(); ++i) {
				placement_shader_->setInt("u_typeIndex", (int)i);
				glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::DecorAllInstances(), decor_types_[i].ssbo);
				glDispatchCompute(dispatch_size, dispatch_size, num_chunks);
			}
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		}
	}

	void DecorManager::Cull(
		const glm::mat4&                      view,
		const glm::mat4&                      projection,
		int                                   viewport_width,
		int                                   viewport_height,
		const std::optional<glm::mat4>&       light_space_matrix,
		const std::optional<glm::vec3>&       light_dir,
		std::shared_ptr<TerrainRenderManager> render_manager
	) {
		PROJECT_PROFILE_SCOPE("DecorManager::Cull");
		if (!enabled_ || !initialized_ || decor_types_.empty())
			return;

		bool is_shadow_pass = light_space_matrix.has_value();

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

		if (light_dir.has_value()) {
			culling_shader_->setVec3("u_lightDir", *light_dir);
			culling_shader_->setVec3("u_cameraPos", 0, 0, 0);
		} else {
			culling_shader_->setVec3("u_lightDir", 0, 0, 0);
			culling_shader_->setVec3("u_cameraPos", camera_pos_);
		}

		if (render_manager) {
			render_manager->BindTerrainData(*culling_shader_);
		}

		culling_shader_->setBool("u_enableHiZ", hiz_enabled_ && !is_shadow_pass);
		if (hiz_enabled_ && !is_shadow_pass) {
			glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::HiZ());
			glBindTexture(GL_TEXTURE_2D, hiz_texture_);
			culling_shader_->setInt("u_hizTexture", Constants::TextureUnit::HiZ());
			culling_shader_->setMat4("u_prevViewProjection", hiz_prev_vp_);
			culling_shader_->setIVec2("u_hizSize", hiz_width_, hiz_height_);
			culling_shader_->setInt("u_hizMipCount", hiz_mip_count_);
		}

		culling_shader_->setInt("u_instancesPerBlock", kInstancesPerChunk);
		block_validity_pb_->BindRange(Constants::SsboBinding::DecorBlockValidity());

		for (auto& type : decor_types_) {
			type.count_pb->AdvanceFrame();
			type.indirect_pb->AdvanceFrame();
			type.shadow_indirect_pb->AdvanceFrame();

			*type.count_pb->GetFrameDataPtr() = 0;

			culling_shader_->use();
			culling_shader_->setVec3("u_aabbMin", type.model->GetData()->aabb.min);
			culling_shader_->setVec3("u_aabbMax", type.model->GetData()->aabb.max);

			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::DecorAllInstances(), type.ssbo);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::DecorInstances(), type.visible_ssbo);
			type.count_pb->BindRange(0);
			glDispatchCompute(kMaxInstancesPerType / 64, 1, 1);

			glMemoryBarrier(GL_ATOMIC_COUNTER_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

			update_commands_shader_->use();
			update_commands_shader_->setInt("u_numCommands", (int)type.model->getMeshes().size());
			type.count_pb->BindRange(0);

			type.indirect_pb->BindRange(Constants::SsboBinding::DecorIndirect());
			glDispatchCompute(1, 1, 1);

			type.shadow_indirect_pb->BindRange(Constants::SsboBinding::DecorIndirect());
			glDispatchCompute(1, 1, 1);

			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
		}
	}

	std::vector<DecorTypeResults> DecorManager::GetDecorInChunks(
		const std::vector<std::pair<int, int>>& chunk_keys,
		std::shared_ptr<TerrainRenderManager>   render_manager,
		const ITerrainGenerator&                terrain_gen
	) {
		PROJECT_PROFILE_SCOPE("DecorManager::GetDecorInChunks");
		if (!render_manager || decor_types_.empty())
			return {};

		float world_scale = terrain_gen.GetWorldScale();
		auto  all_chunks = render_manager->GetDecorChunkData(world_scale);

		std::vector<TerrainRenderManager::DecorChunkData> requested_chunk_data;
		for (const auto& key : chunk_keys) {
			auto it = std::find_if(all_chunks.begin(), all_chunks.end(), [&](const auto& cd) { return cd.key == key; });
			if (it != all_chunks.end()) {
				requested_chunk_data.push_back(*it);
			}
		}

		if (requested_chunk_data.empty())
			return {};

		int num_requested_chunks = (int)requested_chunk_data.size();

		GLuint temp_instance_ssbo;
		glGenBuffers(1, &temp_instance_ssbo);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, temp_instance_ssbo);
		glBufferData(
			GL_SHADER_STORAGE_BUFFER,
			num_requested_chunks * kInstancesPerChunk * sizeof(glm::mat4),
			nullptr,
			GL_DYNAMIC_DRAW
		);

		GLuint temp_chunk_params_ssbo;
		glGenBuffers(1, &temp_chunk_params_ssbo);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, temp_chunk_params_ssbo);
		std::vector<ChunkParamsGPU> chunk_gpu(num_requested_chunks);
		for (int i = 0; i < num_requested_chunks; ++i) {
			const auto& cd = requested_chunk_data[i];
			chunk_gpu[i].offset_slice_size = glm::vec4(cd.world_offset, cd.slice, cd.chunk_size);
			chunk_gpu[i].indices = glm::ivec4(i * kInstancesPerChunk, 0, 0, 0);
		}
		glBufferData(
			GL_SHADER_STORAGE_BUFFER,
			num_requested_chunks * sizeof(ChunkParamsGPU),
			chunk_gpu.data(),
			GL_STREAM_DRAW
		);

		PlacementGlobalsGPU globals;
		globals.camera_and_scale = glm::vec4(0.0f, 0.0f, world_scale, terrain_gen.GetMaxHeight());
		globals.distance_params = glm::vec4(1e6f, 2e6f, 3e6f, (float)kMaxInstancesPerType);

		GLuint temp_globals_ubo;
		glGenBuffers(1, &temp_globals_ubo);
		glBindBuffer(GL_UNIFORM_BUFFER, temp_globals_ubo);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(PlacementGlobalsGPU), &globals, GL_STREAM_DRAW);

		placement_shader_->use();
		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainHeightmap());
		glBindTexture(GL_TEXTURE_2D_ARRAY, render_manager->GetHeightmapTexture());
		placement_shader_->setInt("u_heightmapArray", Constants::TextureUnit::TerrainHeightmap());

		glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainBiomeMap());
		glBindTexture(GL_TEXTURE_2D_ARRAY, render_manager->GetBiomeTexture());
		placement_shader_->setInt("u_biomeMap", Constants::TextureUnit::TerrainBiomeMap());

		glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::DecorProps(), decor_props_ubo_);
		glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::DecorPlacementGlobals(), temp_globals_ubo);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::DecorChunkParams(), temp_chunk_params_ssbo);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::DecorAllInstances(), temp_instance_ssbo);

		int                           num_types = (int)decor_types_.size();
		std::vector<DecorTypeResults> results(num_types);
		size_t                        instances_per_type = num_requested_chunks * kInstancesPerChunk;

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, temp_instance_ssbo);
		glBufferData(
			GL_SHADER_STORAGE_BUFFER,
			num_types * instances_per_type * sizeof(glm::mat4),
			nullptr,
			GL_DYNAMIC_DRAW
		);

		for (int i = 0; i < num_types; ++i) {
			results[i].model_path = decor_types_[i].model->GetModelPath();

			for (int j = 0; j < num_requested_chunks; ++j) {
				const auto& cd = requested_chunk_data[j];
				chunk_gpu[j].indices.x = (i * num_requested_chunks + j) * kInstancesPerChunk;
			}
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, temp_chunk_params_ssbo);
			glBufferSubData(
				GL_SHADER_STORAGE_BUFFER,
				0,
				num_requested_chunks * sizeof(ChunkParamsGPU),
				chunk_gpu.data()
			);

			placement_shader_->setInt("u_typeIndex", i);
			int dispatch_size = (Constants::Class::Terrain::ChunkSize() + 7) / 8;
			glDispatchCompute(dispatch_size, dispatch_size, num_requested_chunks);
		}

		glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

		std::vector<glm::mat4> all_matrices(num_types * instances_per_type);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, temp_instance_ssbo);
		glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, all_matrices.size() * sizeof(glm::mat4), all_matrices.data());

		for (int i = 0; i < num_types; ++i) {
			for (size_t j = 0; j < instances_per_type; ++j) {
				const auto& m = all_matrices[i * instances_per_type + j];
				if (m[3][3] < 0.5f)
					continue;

				DecorInstance inst;
				glm::vec3     skew;
				glm::vec4     perspective;
				glm::decompose(m, inst.scale, inst.rotation, inst.center, skew, perspective);
				inst.aabb = decor_types_[i].model->GetData()->aabb.Transform(m);
				results[i].instances.push_back(inst);
			}
		}

		glDeleteBuffers(1, &temp_instance_ssbo);
		glDeleteBuffers(1, &temp_chunk_params_ssbo);
		glDeleteBuffers(1, &temp_globals_ubo);

		return results;
	}

	void DecorManager::Render(
		const glm::mat4&                      view,
		const glm::mat4&                      projection,
		std::shared_ptr<TerrainRenderManager> render_manager,
		const std::optional<glm::mat4>&       light_space_matrix,
		Shader*                               shader_override
	) {
		PROJECT_PROFILE_SCOPE("DecorManager::Render");
		if (!enabled_ || !initialized_ || decor_types_.empty())
			return;

		bool is_shadow_pass = light_space_matrix.has_value();

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
		shader->setBool("uUseMDI", false);
		shader->setBool("isArcadeText", false);
		shader->setBool("isLine", false);
		shader->setBool("isColossal", false);
		shader->setBool("is_instanced", false);
		shader->setVec3("objectColor", 1.0f, 1.0f, 1.0f);
		shader->setBool("usePBR", false);
		shader->setBool("use_skinning", false);
		shader->setInt("bone_matrices_offset", -1);
		shader->setVec4("clipPlane", 0.0f, 0.0f, 0.0f, 0.0f);
		shader->setFloat(
			"ripple_strength",
			ConfigManager::GetInstance().GetAppSettingBool("artistic_effect_ripple", false) ? 0.05f : 0.0f
		);

		if (render_manager) {
			render_manager->BindTerrainData(*shader);
		}

		if (atmosphere_manager_) {
			atmosphere_manager_->BindToShader(*shader);
		}

		if (noise_manager_) {
			noise_manager_->BindDefault(*shader);
		}

		for (size_t i = 0; i < decor_types_.size(); ++i) {
			auto& type = decor_types_[i];

			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::DecorInstances(), type.visible_ssbo);

			if (type.model->IsNoCull()) {
				glDisable(GL_CULL_FACE);
			}

			shader->setVec3("u_aabbMin", type.model->GetData()->aabb.min);
			shader->setVec3("u_aabbMax", type.model->GetData()->aabb.max);
			shader->setFloat("u_windResponsiveness", type.props.wind_responsiveness);
			shader->setFloat("u_windRimHighlight", type.props.wind_rim_highlight);

			if (is_shadow_pass) {
				glBindBuffer(GL_DRAW_INDIRECT_BUFFER, type.shadow_indirect_pb->GetBufferId());
			} else {
				glBindBuffer(GL_DRAW_INDIRECT_BUFFER, type.indirect_pb->GetBufferId());
			}

			uint32_t indirect_offset =
				is_shadow_pass ? type.shadow_indirect_pb->GetFrameOffset() : type.indirect_pb->GetFrameOffset();

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
				shader->setInt("use_texture", hasDiffuse ? 1 : 0);
				shader->setBool("useVertexColor", mesh.has_vertex_colors && !hasDiffuse);
				mesh.bindTextures(*shader);

				glBindVertexArray(mesh.getVAO());
				glDrawElementsIndirect(
					GL_TRIANGLES,
					GL_UNSIGNED_INT,
					(void*)(uintptr_t)(indirect_offset + mi * sizeof(DrawElementsIndirectCommand))
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
