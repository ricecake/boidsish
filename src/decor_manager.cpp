#include "decor_manager.h"

#include <algorithm>
#include <set>

#include "ConfigManager.h"
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
		if (block_validity_ssbo_ != 0)
			glDeleteBuffers(1, &block_validity_ssbo_);
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

		// Block validity buffer: one uint per block, all initially invalid (0).
		glGenBuffers(1, &block_validity_ssbo_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, block_validity_ssbo_);
		std::vector<uint32_t> validity(kMaxActiveChunks, 0);
		glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxActiveChunks * sizeof(uint32_t), validity.data(), GL_DYNAMIC_DRAW);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		// Global placement params UBO (small, updated per dispatch)
		glGenBuffers(1, &placement_globals_ubo_);
		glBindBuffer(GL_UNIFORM_BUFFER, placement_globals_ubo_);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(PlacementGlobalsGPU), nullptr, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		// Per-chunk params SSBO (resized as needed per dispatch)
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

		// Main instance storage (persistent)
		glGenBuffers(1, &type.ssbo);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, type.ssbo);
		glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxInstancesPerType * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);

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
			{.min_density = 0.01f,
		     .max_density = 0.02f,
		     .base_scale = 0.015f,
		     .scale_variance = 0.01f,
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
		     .base_scale = 0.05f,
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
			{.min_density = 0.15f,
		     .max_density = 0.25f,
		     .base_scale = 0.5f,
		     .scale_variance = 0.1f,
		     .min_height = 0.01f,
		     .max_height = 200.0f,
		     .random_yaw = true,
		     .align_to_terrain = true,
		     .biomes = {Biome::LushGrass, Biome::DryGrass, Biome::Forest, Biome::AlpineMeadow},
		     .wind_responsiveness = 1,
		     .wind_rim_highlight = 1.0f},
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
		for (auto& type : decor_types_) {
			if (mb) {
				type.model->PrepareResources(mb);
			}

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

		if (decor_types_.empty())
			return;

		// Build and upload per-type properties UBO for the placement shader.
		// This replaces 16+ uniform calls per type with a single UBO bind.
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
		float                                  delta_time,
		const Camera&                          camera,
		const Frustum&                         frustum,
		const ITerrainGenerator&               terrain_gen,
		std::shared_ptr<ITerrainRenderManager> render_manager
	) {
		PROJECT_PROFILE_SCOPE("DecorManager::Update");
		if (!enabled_ || !initialized_ || decor_types_.empty())
			return;

		if (!placement_shader_ || !placement_shader_->isValid() || !culling_shader_ || !culling_shader_->isValid())
			return;

		if (!render_manager)
			return;

		frame_counter_++;
		camera_pos_ = camera.pos();
		glm::vec3 fwd = camera.front();
		glm::vec2 new_forward = glm::normalize(glm::vec2(fwd.x, fwd.z));
		camera_rotation_delta_ = new_forward - prev_camera_forward_2d_;
		prev_camera_forward_2d_ = camera_forward_2d_;
		camera_forward_2d_ = new_forward;
		_UpdateAllocation(camera, frustum, terrain_gen, render_manager);
	}

	void DecorManager::_UpdateAllocation(
		const Camera&                          camera,
		const Frustum&                         frustum,
		const ITerrainGenerator&               terrain_gen,
		std::shared_ptr<ITerrainRenderManager> render_manager
	) {
		auto* concrete_rm = dynamic_cast<TerrainRenderManager*>(render_manager.get());
		if (!concrete_rm)
			return;

		float world_scale = terrain_gen.GetWorldScale();

		GLuint heightmap_texture = concrete_rm->GetHeightmapTexture();
		GLuint biome_texture = concrete_rm->GetBiomeTexture();
		if (heightmap_texture == 0 || biome_texture == 0)
			return;

		// Single call: gets keys, offsets, slices, sizes, and update counts
		// in one mutex acquisition with no intermediate map construction.
		auto all_chunks = concrete_rm->GetDecorChunkData(world_scale);
		if (all_chunks.empty())
			return;

		// 1. Filter by distance — no frustum check (GPU cull handles that).
		// Build a flat lookup by index for the dispatch phase.
		glm::vec2 cam_xz(camera.x, camera.z);
		float     max_dist = max_decor_distance_ * world_scale;

		// Reuse a flat vector of indices into all_chunks for the active set
		struct CandidateChunk {
			int   src_index; // index into all_chunks
			float priority;  // lower = generate sooner (front-of-camera bias)
		};

		std::vector<CandidateChunk> candidates;
		candidates.reserve(all_chunks.size());

		// Predict where the camera will be looking based on current rotation.
		// Chunks in the turn direction get a priority boost.
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
			float     facing = glm::dot(camera_forward_2d_, to_chunk); // 1=ahead, -1=behind

			// Aggressive nonlinear penalty: ahead chunks are cheap, behind are expensive.
			// facing  1.0 (ahead)  → weight ~0.3
			// facing  0.0 (side)   → weight ~2.0
			// facing -1.0 (behind) → weight ~5.0
			float facing_weight;
			if (facing > 0.0f) {
				facing_weight = 0.3f + 1.7f * (1.0f - facing); // 0.3 → 2.0
			} else {
				facing_weight = 2.0f + 3.0f * (-facing); // 2.0 → 5.0
			}

			// Rotation prediction: reduce priority for chunks the camera is turning toward.
			// The dot of rotation_dir with to_chunk is positive when turning towards it.
			if (rotation_magnitude > 0.001f) {
				float turn_alignment = glm::dot(rotation_dir, to_chunk);
				// Scale by rotation speed — faster turns get stronger prediction
				float turn_boost = turn_alignment * std::min(rotation_magnitude * 30.0f, 1.5f);
				facing_weight -= turn_boost; // negative turn_boost = chunk behind the turn
				facing_weight = std::max(facing_weight, 0.1f);
			}

			candidates.push_back({i, dist * facing_weight});
		}

		// 2. Mark all candidate chunks as seen; evict stale chunks.
		// Use a flat sorted vector for presence checking instead of std::set.
		std::vector<std::pair<int, int>> active_keys;
		active_keys.reserve(candidates.size());
		for (const auto& c : candidates)
			active_keys.push_back(all_chunks[c.src_index].key);
		std::sort(active_keys.begin(), active_keys.end());

		int chunks_freed = 0;

		for (auto it = active_chunks_.begin(); it != active_chunks_.end();) {
			bool is_present = std::binary_search(active_keys.begin(), active_keys.end(), it->first);

			if (is_present) {
				it->second.last_seen_frame = frame_counter_;
				++it;
			} else if (frame_counter_ - it->second.last_seen_frame > kChunkGracePeriodFrames) {
				int block = it->second.block_index;
				free_blocks_.push_back(block);

				// Mark block invalid in the validity buffer (4 bytes, not 64KB×6 types).
				// The cull shader checks this and skips instances in invalid blocks.
				uint32_t zero = 0;
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, block_validity_ssbo_);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, block * sizeof(uint32_t), sizeof(uint32_t), &zero);

				it = active_chunks_.erase(it);
				chunks_freed++;
			} else {
				++it;
			}
		}

		// 3. Sort candidates by priority (front-of-camera + close first).
		std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
			return a.priority < b.priority;
		});

		// 4. Allocate and collect chunks to generate, with per-frame cap.
		struct GenerateEntry {
			int src_index; // index into all_chunks (for dispatch data)
			int block;     // SSBO block index
		};

		std::vector<GenerateEntry> chunks_to_generate;
		int                        chunks_new = 0;
		int                        chunks_deform_dirty = 0;

		for (const auto& cand : candidates) {
			const auto& cd = all_chunks[cand.src_index];
			auto        it = active_chunks_.find(cd.key);

			if (it == active_chunks_.end()) {
				if (chunks_new >= kMaxNewChunksPerFrame)
					continue; // defer to next frame
				if (free_blocks_.empty())
					continue;

				int block = free_blocks_.back();
				free_blocks_.pop_back();
				active_chunks_[cd.key] = {block, cd.update_count, true, frame_counter_};
				chunks_to_generate.push_back({cand.src_index, block});
				chunks_new++;
			} else if (it->second.update_count != cd.update_count) {
				it->second.update_count = cd.update_count;
				it->second.is_dirty = true;
				chunks_to_generate.push_back({cand.src_index, it->second.block_index});
				chunks_deform_dirty++;
			}
		}

		// if (!chunks_to_generate.empty()) {
		// 	logger::WARNING(
		// 		"Decor placement: generating {} chunks (new={}, deform_dirty={}, freed={}, active={}, free_blocks={})",
		// 		chunks_to_generate.size(),
		// 		chunks_new,
		// 		chunks_deform_dirty,
		// 		chunks_freed,
		// 		active_chunks_.size(),
		// 		free_blocks_.size()
		// 	);
		// }

		// 5. Dispatch placement compute for new/dirty chunks.
		if (!chunks_to_generate.empty()) {
			int num_chunks = (int)chunks_to_generate.size();

			// Mark all blocks being generated as valid in one batch.
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, block_validity_ssbo_);
			for (const auto& entry : chunks_to_generate) {
				uint32_t one = 1;
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, entry.block * sizeof(uint32_t), sizeof(uint32_t), &one);
			}

			// Upload global placement params (once per dispatch frame)
			PlacementGlobalsGPU globals;
			globals.camera_and_scale = glm::vec4(cam_xz, world_scale, terrain_gen.GetMaxHeight());
			globals.distance_params = glm::vec4(
				density_falloff_start_ * world_scale,
				density_falloff_end_ * world_scale,
				max_decor_distance_ * world_scale,
				static_cast<float>(kMaxInstancesPerType)
			);
			glBindBuffer(GL_UNIFORM_BUFFER, placement_globals_ubo_);
			glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(PlacementGlobalsGPU), &globals);

			// Upload per-chunk params SSBO (all chunks in one buffer)
			std::vector<ChunkParamsGPU> chunk_gpu(num_chunks);
			for (int j = 0; j < num_chunks; ++j) {
				const auto& cd = all_chunks[chunks_to_generate[j].src_index];
				chunk_gpu[j].offset_slice_size = glm::vec4(cd.world_offset, cd.slice, cd.chunk_size);
				chunk_gpu[j].indices = glm::ivec4(chunks_to_generate[j].block * kInstancesPerChunk, 0, 0, 0);
			}
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunk_params_ssbo_);
			glBufferData(
				GL_SHADER_STORAGE_BUFFER,
				num_chunks * sizeof(ChunkParamsGPU),
				chunk_gpu.data(),
				GL_STREAM_DRAW
			);

			// Bind everything once, then one dispatch per type
			placement_shader_->use();

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture);
			placement_shader_->setInt("u_heightmapArray", 0);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D_ARRAY, biome_texture);
			placement_shader_->setInt("u_biomeMap", 1);

			glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::DecorProps(), decor_props_ubo_);
			glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::DecorPlacementGlobals(), placement_globals_ubo_);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::DecorChunkParams(), chunk_params_ssbo_);

			for (size_t i = 0; i < decor_types_.size(); ++i) {
				placement_shader_->setInt("u_typeIndex", (int)i);
				glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, decor_types_[i].ssbo);
				glDispatchCompute(4, 4, num_chunks); // Z = chunk index
			}
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		}
	}

	void DecorManager::Cull(
		const glm::mat4&                       view,
		const glm::mat4&                       projection,
		int                                    viewport_width,
		int                                    viewport_height,
		const std::optional<glm::mat4>&        light_space_matrix,
		const std::optional<glm::vec3>&        light_dir,
		std::shared_ptr<ITerrainRenderManager> render_manager
	) {
		PROJECT_PROFILE_SCOPE("DecorManager::Cull");
		if (!enabled_ || !initialized_ || decor_types_.empty())
			return;

		bool is_shadow_pass = light_space_matrix.has_value();

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

		if (light_dir.has_value()) {
			culling_shader_->setVec3("u_lightDir", *light_dir);
			culling_shader_->setVec3("u_cameraPos", 0, 0, 0);
		} else {
			culling_shader_->setVec3("u_lightDir", 0, 0, 0);
			culling_shader_->setVec3("u_cameraPos", camera_pos_);
		}

		if (render_manager) {
			if (auto* tm = dynamic_cast<TerrainRenderManager*>(render_manager.get())) {
				tm->BindTerrainData(*culling_shader_);
			}
		}

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

		// Bind block validity buffer once for all types (shared allocation scheme)
		culling_shader_->setInt("u_instancesPerBlock", kInstancesPerChunk);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, block_validity_ssbo_);

		for (auto& type : decor_types_) {
			// Reset atomic counter
			unsigned int zero = 0;
			glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, type.count_buffer);
			glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(unsigned int), &zero);

			// Cull instances
			culling_shader_->use();
			culling_shader_->setVec3("u_aabbMin", type.model->GetData()->aabb.min);
			culling_shader_->setVec3("u_aabbMax", type.model->GetData()->aabb.max);

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
	}

	std::vector<DecorTypeResults> DecorManager::GetDecorInChunks(
		const std::vector<std::pair<int, int>>& chunk_keys,
		std::shared_ptr<ITerrainRenderManager>  render_manager,
		const ITerrainGenerator&                terrain_gen
	) {
		PROJECT_PROFILE_SCOPE("DecorManager::GetDecorInChunks");
		auto* concrete_rm = dynamic_cast<TerrainRenderManager*>(render_manager.get());
		if (!concrete_rm || decor_types_.empty())
			return {};

		float world_scale = terrain_gen.GetWorldScale();
		auto  all_chunks = concrete_rm->GetDecorChunkData(world_scale);

		// 1. Find the chunk data for the requested keys
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

		// 2. Prepare temporary GPU resources
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
		// Use a dummy camera position and large distances to ensure all decor is generated.
		// Avoid equal values for falloff parameters to prevent smoothstep(a, a, x) issues.
		globals.camera_and_scale = glm::vec4(0.0f, 0.0f, world_scale, terrain_gen.GetMaxHeight());
		globals.distance_params = glm::vec4(1e6f, 2e6f, 3e6f, (float)kMaxInstancesPerType);

		GLuint temp_globals_ubo;
		glGenBuffers(1, &temp_globals_ubo);
		glBindBuffer(GL_UNIFORM_BUFFER, temp_globals_ubo);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(PlacementGlobalsGPU), &globals, GL_STREAM_DRAW);

		// 3. Dispatch placement for each type
		placement_shader_->use();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D_ARRAY, concrete_rm->GetHeightmapTexture());
		placement_shader_->setInt("u_heightmapArray", 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D_ARRAY, concrete_rm->GetBiomeTexture());
		placement_shader_->setInt("u_biomeMap", 1);

		glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::DecorProps(), decor_props_ubo_);
		glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::DecorPlacementGlobals(), temp_globals_ubo);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::DecorChunkParams(), temp_chunk_params_ssbo);
		// Note: decor_placement.comp uses binding 0 for DecorInstances
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, temp_instance_ssbo);

		int                           num_types = (int)decor_types_.size();
		std::vector<DecorTypeResults> results(num_types);
		size_t                        instances_per_type = num_requested_chunks * kInstancesPerChunk;

		// Re-allocate temp buffer if it's too small for all types
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, temp_instance_ssbo);
		glBufferData(
			GL_SHADER_STORAGE_BUFFER,
			num_types * instances_per_type * sizeof(glm::mat4),
			nullptr,
			GL_DYNAMIC_DRAW
		);

		for (int i = 0; i < num_types; ++i) {
			results[i].model_path = decor_types_[i].model->GetModelPath();

			// Update chunk params for this type's offset
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
			glDispatchCompute(4, 4, num_requested_chunks);
		}

		glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

		// 4. Single read back and process
		std::vector<glm::mat4> all_matrices(num_types * instances_per_type);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, temp_instance_ssbo);
		glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, all_matrices.size() * sizeof(glm::mat4), all_matrices.data());

		for (int i = 0; i < num_types; ++i) {
			for (size_t j = 0; j < instances_per_type; ++j) {
				const auto& m = all_matrices[i * instances_per_type + j];
				if (m[3][3] < 0.5f)
					continue; // Not a valid instance (scale is 0)

				DecorInstance inst;
				glm::vec3     skew;
				glm::vec4     perspective;
				glm::decompose(m, inst.scale, inst.rotation, inst.center, skew, perspective);

				// Calculate world-space AABB
				inst.aabb = decor_types_[i].model->GetData()->aabb.Transform(m);

				results[i].instances.push_back(inst);
			}
		}

		// 5. Cleanup
		glDeleteBuffers(1, &temp_instance_ssbo);
		glDeleteBuffers(1, &temp_chunk_params_ssbo);
		glDeleteBuffers(1, &temp_globals_ubo);

		return results;
	}

	void DecorManager::Render(
		const glm::mat4&                view,
		const glm::mat4&                projection,
		const std::optional<glm::mat4>& light_space_matrix,
		Shader*                         shader_override
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

			shader->setVec3("u_aabbMin", type.model->GetData()->aabb.min);
			shader->setVec3("u_aabbMax", type.model->GetData()->aabb.max);
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
				shader->setInt("use_texture", hasDiffuse ? 1 : 0);
				shader->setBool("useVertexColor", mesh.has_vertex_colors && !hasDiffuse);
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
