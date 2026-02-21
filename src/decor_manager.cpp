#include "decor_manager.h"

#include <algorithm>
#include <set>

#include "ConfigManager.h"
#include "graphics.h"
#include "logger.h"
#include "terrain_generator_interface.h"
#include "terrain_render_manager.h"
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>

namespace Boidsish {

	DecorManager::DecorManager() {}

	struct DrawElementsIndirectCommand {
		unsigned int count;
		unsigned int instanceCount;
		unsigned int firstIndex;
		unsigned int baseVertex;
		unsigned int baseInstance;
	};

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
		_Initialize();

		DecorType type;
		type.model = std::make_shared<Model>(model_path);
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

		// Atomic counter for culling
		glGenBuffers(1, &type.count_buffer);
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, type.count_buffer);
		glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(unsigned int), nullptr, GL_DYNAMIC_DRAW);

		decor_types_.push_back(type);
	}

	void DecorManager::Update(
		float /*delta_time*/,
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
		float  world_scale = terrain_gen.GetWorldScale();
		GLuint heightmap_texture = render_manager->GetHeightmapTexture();
		if (heightmap_texture == 0)
			return;

		auto chunk_info = render_manager->GetChunkInfo(world_scale);
		if (chunk_info.empty())
			return;

		uint32_t current_terrain_version = terrain_gen.GetVersion();

		// 1. Identify visible chunks and their distances
		std::vector<std::pair<float, std::pair<int, int>>> visible_chunks;
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

			if (!frustum.IsBoxInFrustum(chunk_min, chunk_max))
				continue;

			// Store key and distance (for priority)
			int cx = static_cast<int>(std::floor(chunk.x / chunk.w + 0.5f));
			int cz = static_cast<int>(std::floor(chunk.y / chunk.w + 0.5f));
			visible_chunks.push_back({dist, {cx, cz}});
		}

		// 2. Cull chunks that are no longer visible or too far
		std::set<std::pair<int, int>> currently_visible_keys;
		for (const auto& vc : visible_chunks)
			currently_visible_keys.insert(vc.second);

		for (auto it = active_chunks_.begin(); it != active_chunks_.end();) {
			if (currently_visible_keys.find(it->first) == currently_visible_keys.end()) {
				free_blocks_.push_back(it->second.block_index);
				it = active_chunks_.erase(it);
			} else {
				++it;
			}
		}

		// 3. Allocate blocks for new visible chunks
		std::sort(visible_chunks.begin(), visible_chunks.end()); // Closer first

		std::vector<std::pair<std::pair<int, int>, int>> chunks_to_generate;

		for (const auto& vc : visible_chunks) {
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
				placement_shader_->setInt("u_typeIndex", (int)i);

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

	void DecorManager::Render(const glm::mat4& view, const glm::mat4& projection) {
		if (!enabled_ || !initialized_ || decor_types_.empty())
			return;

		// 1. GPU Culling Pass
		Frustum frustum = Frustum::FromViewProjection(view, projection);

		culling_shader_->use();
		for (int p = 0; p < 6; ++p) {
			culling_shader_->setVec4(
				"u_frustumPlanes[" + std::to_string(p) + "]",
				glm::vec4(frustum.planes[p].normal, frustum.planes[p].distance)
			);
		}
		culling_shader_->setInt("u_totalSlots", kMaxInstancesPerType);

		for (auto& type : decor_types_) {
			// Reset atomic counter
			unsigned int zero = 0;
			glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, type.count_buffer);
			glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(unsigned int), &zero);

			// Cull instances
			culling_shader_->use();
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, type.ssbo);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, type.visible_ssbo);
			glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, type.count_buffer);
			glDispatchCompute(kMaxInstancesPerType / 64, 1, 1);

			glMemoryBarrier(GL_ATOMIC_COUNTER_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

			// Update indirect commands
			update_commands_shader_->use();
			update_commands_shader_->setInt("u_numCommands", (int)type.model->getMeshes().size());
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, type.indirect_buffer);
			glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, type.count_buffer);
			glDispatchCompute(1, 1, 1);

			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
		}

		// 2. Rendering Pass
		auto shader = Shape::shader;
		if (!shader)
			return;

		shader->use();
		shader->setMat4("view", view);
		shader->setMat4("projection", projection);
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

			// Bind the indirect buffer
			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, type.indirect_buffer);

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
				mesh.bindTextures(*shader);

				glBindVertexArray(mesh.VAO);
				glDrawElementsIndirect(
					GL_TRIANGLES,
					GL_UNSIGNED_INT,
					(void*)(mi * sizeof(DrawElementsIndirectCommand))
				);
			}
		}

		glBindVertexArray(0);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
		shader->setBool("useSSBOInstancing", false);
	}

} // namespace Boidsish
