#include "fire_effect_manager.h"

#include <algorithm>

#include "ConfigManager.h"
#include "service_locator.h"
#include <numeric>
#include <queue>

#include "atmosphere_manager.h"
#include "biome_properties.h"
#include "graphics.h"
#include "logger.h"
#include "model.h"
#include "profiler.h"
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>

namespace Boidsish {

	struct Particle {
		glm::vec4 pos;
		glm::vec4 vel;
		alignas(16) glm::vec3 epicenter;
		int   style;
		int   emitter_index;
		int   emitter_id;
		float extras[2];
	};

	FireEffectManager::FireEffectManager(ServiceLocator& /*loc*/) {}

	void FireEffectManager::Initialize() {
		_EnsureShaderAndBuffers();
	}

	bool FireEffectManager::IsAvailable() const {
		return initialized_ && lifecycle_shader_ && lifecycle_shader_->isValid() && behavior_shader_ &&
			behavior_shader_->isValid() && fixup_shader_ && fixup_shader_->isValid();
	}

	FireEffectManager::~FireEffectManager() {
		if (particle_buffer_ != 0) {
			glDeleteBuffers(1, &particle_buffer_);
		}
		if (visible_indices_buffer_ != 0) {
			glDeleteBuffers(1, &visible_indices_buffer_);
		}
		if (live_indices_buffer_ != 0) {
			glDeleteBuffers(1, &live_indices_buffer_);
		}
		if (draw_command_buffer_ != 0) {
			glDeleteBuffers(1, &draw_command_buffer_);
		}
		if (behavior_command_buffer_ != 0) {
			glDeleteBuffers(1, &behavior_command_buffer_);
		}
		stats_buffer_.reset();
		if (grid_heads_buffer_ != 0) {
			glDeleteBuffers(1, &grid_heads_buffer_);
		}
		if (grid_next_buffer_ != 0) {
			glDeleteBuffers(1, &grid_next_buffer_);
		}
		emitter_buffer_.reset();
		indirection_buffer_.reset();
		terrain_chunk_pb_.reset();
		slice_data_pb_.reset();
		if (dummy_vao_ != 0) {
			glDeleteVertexArrays(1, &dummy_vao_);
		}
	}

	void FireEffectManager::_EnsureShaderAndBuffers() {
		if (initialized_) {
			return;
		}

		lifecycle_shader_ = std::make_unique<ComputeShader>("shaders/fire_lifecycle.comp");
		if (!lifecycle_shader_->isValid()) {
			logger::ERROR("Failed to compile fire lifecycle shader - fire effects will be disabled");
			initialized_ = true;
			return;
		}

		behavior_shader_ = std::make_unique<ComputeShader>("shaders/fire_behavior.comp");
		if (!behavior_shader_->isValid()) {
			logger::ERROR("Failed to compile fire behavior shader - fire effects will be disabled");
			initialized_ = true;
			return;
		}

		fixup_shader_ = std::make_unique<ComputeShader>("shaders/particle_command_fixup.comp");
		if (!fixup_shader_->isValid()) {
			logger::ERROR("Failed to compile particle fixup shader - fire effects will be disabled");
			initialized_ = true;
			return;
		}

		grid_build_shader_ = std::make_unique<ComputeShader>("shaders/particle_grid_build.comp");
		if (!grid_build_shader_->isValid()) {
			logger::ERROR("Failed to compile particle grid build shader");
		}

		render_shader_ = std::make_unique<Shader>("shaders/fire.vert", "shaders/fire.frag");

		render_shader_->use();
		render_shader_->bindUniformBlock("FrustumData", Constants::UboBinding::FrustumData());
		render_shader_->bindUniformBlock("Lighting", Constants::UboBinding::Lighting());
		render_shader_->bindUniformBlock("TemporalData", Constants::UboBinding::TemporalData());

		auto setup_comp_ubos = [&](ComputeShader* shader) {
			shader->use();
			shader->bindUniformBlock("Lighting", Constants::UboBinding::Lighting());
			shader->bindUniformBlock("FrustumData", Constants::UboBinding::FrustumData());
			shader->bindUniformBlock("VisualEffects", Constants::UboBinding::VisualEffects());
		};

		setup_comp_ubos(lifecycle_shader_.get());
		setup_comp_ubos(behavior_shader_.get());

		glGenBuffers(1, &particle_buffer_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, particle_buffer_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxParticles * sizeof(Particle), nullptr, GL_DYNAMIC_DRAW);
		std::vector<uint8_t> zero_data(kMaxParticles * sizeof(Particle), 0);
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, zero_data.size(), zero_data.data());

		glGenBuffers(1, &grid_heads_buffer_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, grid_heads_buffer_);
		glBufferData(
			GL_SHADER_STORAGE_BUFFER,
			Constants::Class::Particles::ParticleGridSize() * sizeof(int),
			nullptr,
			GL_DYNAMIC_DRAW
		);

		glGenBuffers(1, &grid_next_buffer_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, grid_next_buffer_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxParticles * sizeof(int), nullptr, GL_DYNAMIC_DRAW);

		emitter_buffer_ = std::make_unique<PersistentBuffer<Emitter>>(GL_SHADER_STORAGE_BUFFER, kMaxEmitters, 3);
		indirection_buffer_ = std::make_unique<PersistentBuffer<int>>(GL_SHADER_STORAGE_BUFFER, kMaxParticles, 3);

		terrain_chunk_pb_ = std::make_unique<PersistentBuffer<glm::vec4>>(GL_SHADER_STORAGE_BUFFER, 1024, 3);
		slice_data_pb_ = std::make_unique<PersistentBuffer<glm::vec4>>(GL_SHADER_STORAGE_BUFFER, kMaxEmitters * 64, 3);

		glGenBuffers(1, &visible_indices_buffer_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, visible_indices_buffer_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxParticles * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);

		glGenBuffers(1, &live_indices_buffer_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, live_indices_buffer_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxParticles * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);

		glGenBuffers(1, &draw_command_buffer_);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, draw_command_buffer_);
		glBufferData(GL_DRAW_INDIRECT_BUFFER, 4 * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);

		glGenBuffers(1, &behavior_command_buffer_);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, behavior_command_buffer_);
		glBufferData(GL_DRAW_INDIRECT_BUFFER, 4 * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);

		stats_buffer_ = std::make_unique<PersistentBuffer<ParticleStats>>(
			GL_SHADER_STORAGE_BUFFER,
			1,
			3,
			GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT
		);

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

		particle_to_emitter_map_.resize(kMaxParticles, -1);
		glGenVertexArrays(1, &dummy_vao_);
		initialized_ = true;
	}

	std::shared_ptr<FireEffect> FireEffectManager::AddEffect(
		const glm::vec3& position,
		FireEffectStyle  style,
		const glm::vec3& direction,
		const glm::vec3& velocity,
		int              max_particles,
		float            lifetime,
		EmitterType      type,
		const glm::vec3& dimensions,
		float            sweep
	) {
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		_EnsureShaderAndBuffers();

		if (!lifecycle_shader_ || !lifecycle_shader_->isValid() || !behavior_shader_ || !behavior_shader_->isValid() ||
		    !fixup_shader_ || !fixup_shader_->isValid()) {
			return nullptr;
		}

		for (size_t i = 0; i < effects_.size(); ++i) {
			if (!effects_[i]) {
				effects_[i] = std::make_shared<
					FireEffect>(position, style, direction, velocity, max_particles, lifetime, type, dimensions, sweep);
				needs_reallocation_ = true;
				return effects_[i];
			}
		}

		if (effects_.size() < kMaxEmitters) {
			auto effect = std::make_shared<
				FireEffect>(position, style, direction, velocity, max_particles, lifetime, type, dimensions, sweep);
			effects_.push_back(effect);
			needs_reallocation_ = true;
			return effect;
		}

		logger::ERROR("Maximum number of fire effects reached.");
		return nullptr;
	}

	void FireEffectManager::RemoveEffect(const std::shared_ptr<FireEffect>& effect) {
		if (effect) {
			std::lock_guard<std::recursive_mutex> lock(mutex_);
			for (size_t i = 0; i < effects_.size(); ++i) {
				if (effects_[i] == effect) {
					effects_[i] = nullptr;
					needs_reallocation_ = true;
					return;
				}
			}
		}
	}

	FireEffectManager::UpdateResult FireEffectManager::PrepareUpdate(float delta_time, float ambient_density) {
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		UpdateResult res;

		if (std::abs(ambient_density - ambient_density_) > 0.001f) {
			ambient_density_ = ambient_density;
			needs_reallocation_ = true;
		}

		for (auto& effect : effects_) {
			if (effect) {
				float lifetime = effect->GetLifetime();
				if (lifetime > 0.0f) {
					float lived = effect->GetLived();
					lived += delta_time;
					effect->SetLived(lived);
					if (lived >= lifetime) {
						effect = nullptr;
						needs_reallocation_ = true;
					}
				}
			}
		}

		if (needs_reallocation_) {
			_UpdateParticleAllocation();
			needs_reallocation_ = false;
		}

		res.particle_to_emitter_map = particle_to_emitter_map_;
		res.emitters.reserve(effects_.size());

		for (const auto& effect : effects_) {
			if (effect) {
				Emitter emitter = {
					effect->GetPosition(),
					(int)effect->GetStyle(),
					effect->GetDirection(),
					effect->IsActive() ? 1 : 0,
					effect->GetVelocity(),
					effect->GetId(),
					effect->GetDimensions(),
					(int)effect->GetType(),
					effect->GetSweep(),
					0, 0, 0, 0.0f,
					effect->NeedsClear() ? 1 : 0,
					{0, 0}
				};

				auto model = effect->GetSourceModel();
				if (effect->GetType() == EmitterType::Model && model) {
					ModelSlice slice = model->GetSlice(effect->GetDirection(), effect->GetSweep());
					emitter.slice_area = slice.area;
					if (!slice.triangles.empty()) {
						emitter.use_slice_data = 1;
						emitter.slice_data_offset = static_cast<int>(res.slice_points.size());
						emitter.slice_data_count = 64;
						for (int i = 0; i < emitter.slice_data_count; ++i) {
							res.slice_points.push_back(glm::vec4(slice.GetRandomPoint(), 1.0f));
						}
					}
				}

				if (effect->NeedsClear()) {
					effect->ResetClearRequest();
				}
				res.emitters.push_back(emitter);
			} else {
				res.emitters.push_back(
					{glm::vec3(0), 0, glm::vec3(0), 0, glm::vec3(0), 0, glm::vec3(0), 0, 0.0f, 0, 0, 0, 0.0f, 0, {0, 0}}
				);
			}
		}
		res.valid = true;
		return res;
	}

	void FireEffectManager::ApplyUpdate(const UpdateResult& res, const std::vector<glm::vec4>& chunk_info) {
		PROJECT_PROFILE_SCOPE("FireEffectManager::ApplyUpdate");

		slice_data_pb_->AdvanceFrame();
		if (!res.slice_points.empty()) {
			size_t count = std::min(res.slice_points.size(), slice_data_pb_->GetElementCount());
			std::memcpy(slice_data_pb_->GetFrameDataPtr(), res.slice_points.data(), count * sizeof(glm::vec4));
		}

		emitter_buffer_->AdvanceFrame();
		if (!res.emitters.empty()) {
			std::memcpy(emitter_buffer_->GetFrameDataPtr(), res.emitters.data(), res.emitters.size() * sizeof(Emitter));
		}

		indirection_buffer_->AdvanceFrame();
		if (!res.particle_to_emitter_map.empty()) {
			std::memcpy(
				indirection_buffer_->GetFrameDataPtr(),
				res.particle_to_emitter_map.data(),
				res.particle_to_emitter_map.size() * sizeof(int)
			);
		}

		terrain_chunk_pb_->AdvanceFrame();
		if (!chunk_info.empty()) {
			size_t count = std::min(chunk_info.size(), terrain_chunk_pb_->GetElementCount());
			std::memcpy(terrain_chunk_pb_->GetFrameDataPtr(), chunk_info.data(), count * sizeof(glm::vec4));
		}

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, draw_command_buffer_);
		uint32_t draw_cmd_init[4] = {0, 1, 0, 0};
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(draw_cmd_init), draw_cmd_init);

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, behavior_command_buffer_);
		uint32_t behavior_cmd_init[4] = {0, 1, 1, 0};
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(behavior_cmd_init), behavior_cmd_init);
	}

	void FireEffectManager::Update(
		float                         delta_time,
		float                         time,
		bool                          enabled,
		float                         ambient_density,
		const std::vector<glm::vec4>& chunk_info,
		const UpdateResult*           precomputed_res,
		GLuint                        heightmap_texture,
		GLuint                        curl_noise_texture,
		GLuint                        biome_texture,
		GLuint                        lighting_ubo,
		GLintptr                      lighting_ubo_offset,
		GLsizeiptr                    lighting_ubo_size,
		GLuint                        frustum_ubo,
		GLintptr                      frustum_offset,
		GLuint                        extra_noise_texture,
		GLuint                        visual_effects_ubo,
		GLintptr                      vfx_offset,
		GLsizeiptr                    vfx_size
	) {
		PROJECT_PROFILE_SCOPE("FireEffectManager::Update");
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		if (!initialized_ || !lifecycle_shader_ || !lifecycle_shader_->isValid() || !behavior_shader_ ||
		    !behavior_shader_->isValid() || !fixup_shader_ || !fixup_shader_->isValid()) {
			return;
		}

		time_ = time;

		if (precomputed_res && precomputed_res->valid) {
			ApplyUpdate(*precomputed_res, chunk_info);
		} else {
			UpdateResult res = PrepareUpdate(delta_time, ambient_density);
			ApplyUpdate(res, chunk_info);
		}

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleBuffer(), particle_buffer_);
		emitter_buffer_->BindRange(Constants::SsboBinding::EmitterBuffer());
		indirection_buffer_->BindRange(Constants::SsboBinding::IndirectionBuffer());
		terrain_chunk_pb_->BindRange(Constants::SsboBinding::TerrainChunkInfo());
		slice_data_pb_->BindRange(Constants::SsboBinding::SliceData());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::VisibleParticleIndices(), visible_indices_buffer_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleDrawCommand(), draw_command_buffer_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::LiveParticleIndices(), live_indices_buffer_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::BehaviorDrawCommand(), behavior_command_buffer_);
		stats_buffer_->AdvanceFrame();
		stats_buffer_->BindRange(Constants::SsboBinding::ParticleStats());

		auto& cfg = ConfigManager::GetInstance();
		ParticleStats* stats_ptr = stats_buffer_->GetFrameDataPtr();
		*stats_ptr = ParticleStats{};
		stats_ptr->limit_birds = cfg.GetAppSettingInt("particle_limit_birds", 1000);
		stats_ptr->limit_leaves = cfg.GetAppSettingInt("particle_limit_leaves", 5000);
		stats_ptr->limit_petals = cfg.GetAppSettingInt("particle_limit_petals", 5000);
		stats_ptr->limit_bubbles = cfg.GetAppSettingInt("particle_limit_bubbles", 2000);
		stats_ptr->limit_fireflies = cfg.GetAppSettingInt("particle_limit_fireflies", 3000);
		stats_ptr->limit_snow = cfg.GetAppSettingInt("particle_limit_snow", 10000);

		bool particles_globally_enabled = cfg.GetAppSettingBool("particles_enabled", true);

		auto bind_textures_and_uniforms = [&](ComputeShader* shader) {
			shader->use();
			shader->setFloat("u_delta_time", delta_time);
			shader->setFloat("u_time", time_);
			shader->setBool("u_enabled", enabled && particles_globally_enabled);
			shader->setFloat("u_ambient_density", ambient_density);
			shader->setInt("u_ambient_particle_scale", Constants::Class::Particles::AmbientParticleScale());
			shader->setInt("u_num_emitters", (int)effects_.size());
			shader->setInt("u_num_chunks", (int)chunk_info.size());
			shader->setUint("u_grid_size", Constants::Class::Particles::ParticleGridSize());
			shader->setFloat("u_cell_size", Constants::Class::Particles::ParticleGridCellSize());

			if (heightmap_texture != 0) {
				glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainHeightmap());
				glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture);
				shader->setInt("u_heightmapArray", Constants::TextureUnit::TerrainHeightmap());
			}
			if (curl_noise_texture != 0) {
				glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::NoiseCurl());
				glBindTexture(GL_TEXTURE_3D, curl_noise_texture);
				shader->setInt("u_curlTexture", Constants::TextureUnit::NoiseCurl());
			}
			if (biome_texture != 0) {
				glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::TerrainBiomeMap());
				glBindTexture(GL_TEXTURE_2D_ARRAY, biome_texture);
				shader->setInt("u_biomeMap", Constants::TextureUnit::TerrainBiomeMap());
			}
			if (extra_noise_texture != 0) {
				glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::NoiseExtra());
				glBindTexture(GL_TEXTURE_3D, extra_noise_texture);
				shader->setInt("u_extraNoiseTexture", Constants::TextureUnit::NoiseExtra());
			}
		};

		if (lighting_ubo != 0) {
			if (lighting_ubo_size > 0) {
				glBindBufferRange(GL_UNIFORM_BUFFER, Constants::UboBinding::Lighting(),
					lighting_ubo, lighting_ubo_offset, lighting_ubo_size);
			} else {
				glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::Lighting(), lighting_ubo);
			}
		}

		if (frustum_ubo != 0) {
			glBindBufferRange(GL_UNIFORM_BUFFER, Constants::UboBinding::FrustumData(),
				frustum_ubo, frustum_offset, sizeof(FrustumDataGPU));
		}

		if (visual_effects_ubo != 0) {
			if (vfx_size > 0) {
				glBindBufferRange(GL_UNIFORM_BUFFER, Constants::UboBinding::VisualEffects(),
					visual_effects_ubo, vfx_offset, vfx_size);
			} else {
				glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::VisualEffects(), visual_effects_ubo);
			}
		}

		bind_textures_and_uniforms(lifecycle_shader_.get());
		glDispatchCompute((kMaxParticles / Constants::Class::Particles::ComputeGroupSize()) + 1, 1, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		if (grid_build_shader_ && grid_build_shader_->isValid()) {
			grid_build_shader_->use();
			grid_build_shader_->setUint("u_grid_size", Constants::Class::Particles::ParticleGridSize());
			grid_build_shader_->setFloat("u_cell_size", Constants::Class::Particles::ParticleGridCellSize());
			grid_build_shader_->setInt("u_num_particles", kMaxParticles);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleBuffer(), particle_buffer_);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleGridHeads(), grid_heads_buffer_);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleGridNext(), grid_next_buffer_);

			grid_build_shader_->setInt("u_mode", 0);
			glDispatchCompute((Constants::Class::Particles::ParticleGridSize() / Constants::Class::Particles::ComputeGroupSize()) + 1, 1, 1);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

			grid_build_shader_->setInt("u_mode", 1);
			glDispatchCompute((kMaxParticles / Constants::Class::Particles::ComputeGroupSize()) + 1, 1, 1);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		}

		fixup_shader_->use();
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::BehaviorDrawCommand(), behavior_command_buffer_);
		glDispatchCompute(1, 1, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

		bind_textures_and_uniforms(behavior_shader_.get());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleGridHeads(), grid_heads_buffer_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleGridNext(), grid_next_buffer_);
		glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, behavior_command_buffer_);
		glDispatchComputeIndirect(0);
		glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, 0);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleBuffer(), 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleGridHeads(), 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleGridNext(), 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::EmitterBuffer(), 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::IndirectionBuffer(), 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::TerrainChunkInfo(), 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::SliceData(), 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::VisibleParticleIndices(), 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleDrawCommand(), 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::LiveParticleIndices(), 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::BehaviorDrawCommand(), 0);
	}

	void FireEffectManager::_UpdateParticleAllocation() {
		int ambient_count = static_cast<int>(ambient_density_ * Constants::Class::Particles::AmbientParticleScale());
		ambient_count = std::clamp(ambient_count, 0, kMaxParticles);
		int emitter_budget = kMaxParticles - ambient_count;

		std::vector<int> ideal_counts(effects_.size(), 0);
		int              total_particle_demand = 0;
		int              num_unlimited_emitters = 0;

		for (const auto& effect : effects_) {
			if (effect) {
				int max_p = effect->GetMaxParticles();
				if (max_p != -1) total_particle_demand += max_p;
				else num_unlimited_emitters++;
			}
		}

		int avg_particles_per_unlimited = 0;
		if (num_unlimited_emitters > 0) {
			int available_for_unlimited = emitter_budget - total_particle_demand;
			if (available_for_unlimited > 0) avg_particles_per_unlimited = available_for_unlimited / num_unlimited_emitters;
		}

		for (size_t i = 0; i < effects_.size(); ++i) {
			if (effects_[i]) {
				int max_p = effects_[i]->GetMaxParticles();
				ideal_counts[i] = (max_p != -1) ? max_p : avg_particles_per_unlimited;
			}
		}

		int current_idx = 0;
		for (size_t i = 0; i < effects_.size(); ++i) {
			if (effects_[i]) {
				int count = ideal_counts[i];
				for (int j = 0; j < count && current_idx < emitter_budget; ++j) {
					particle_to_emitter_map_[current_idx++] = (int)i;
				}
			}
		}
		while (current_idx < emitter_budget) particle_to_emitter_map_[current_idx++] = -1;
		for (int i = emitter_budget; i < kMaxParticles; ++i) particle_to_emitter_map_[i] = -1;
	}

	ParticleStats FireEffectManager::GetStats() const {
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		ParticleStats               stats = {};
		if (stats_buffer_) {
			int prev_idx = (stats_buffer_->GetCurrentBufferIndex() + 2) % 3;
			stats = *stats_buffer_->GetFrameDataPtr(prev_idx);
		}
		return stats;
	}

	void FireEffectManager::BindBuffers(ShaderBase& /*shader*/) const {
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		if (!initialized_) return;
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleBuffer(), particle_buffer_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleGridHeads(), grid_heads_buffer_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleGridNext(), grid_next_buffer_);
	}

	void FireEffectManager::Render(
		const glm::mat4& view,
		const glm::mat4& projection,
		const glm::vec3& camera_pos,
		GLuint           noise_texture,
		GLuint           extra_noise_texture
	) {
		PROJECT_PROFILE_SCOPE("FireEffectManager::Render");
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		if (!initialized_ || !render_shader_) return;
		auto atmos_mgr = ServiceLocator::Instance().Get<AtmosphereManager>();
		if (!ConfigManager::GetInstance().GetAppSettingBool("particles_enabled", true)) return;

		glEnable(GL_BLEND);
		glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
		glDepthMask(GL_FALSE);
		glEnable(GL_PROGRAM_POINT_SIZE);

		render_shader_->use();
		render_shader_->setMat4("u_view", view);
		render_shader_->setMat4("u_projection", projection);
		render_shader_->setVec3("u_camera_pos", camera_pos);
		render_shader_->setFloat("u_time", time_);

		if (atmos_mgr) atmos_mgr->BindToShader(*render_shader_);
		for (int i = 0; i < static_cast<int>(Biome::Count); ++i) {
			render_shader_->setVec3("u_biomeAlbedos[" + std::to_string(i) + "]", kBiomes[i].albedo);
		}

		render_shader_->bindUniformBlock("Lighting", Constants::UboBinding::Lighting());
		emitter_buffer_->BindRange(Constants::SsboBinding::EmitterBuffer());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::VisibleParticleIndices(), visible_indices_buffer_);

		if (noise_texture != 0) {
			glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::NoiseSimplex());
			glBindTexture(GL_TEXTURE_3D, noise_texture);
			render_shader_->setInt("u_noiseTexture", Constants::TextureUnit::NoiseSimplex());
		}
		if (extra_noise_texture != 0) {
			glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::NoiseExtra());
			glBindTexture(GL_TEXTURE_3D, extra_noise_texture);
			render_shader_->setInt("u_extraNoiseTexture", Constants::TextureUnit::NoiseExtra());
		}

		render_shader_->setBool("enableFrustumCulling", true);
		render_shader_->setFloat("frustumCullRadius", 2.0f);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleBuffer(), particle_buffer_);
		glBindVertexArray(dummy_vao_);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, draw_command_buffer_);
		glDrawArraysIndirect(GL_POINTS, 0);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
		glBindVertexArray(0);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleBuffer(), 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::EmitterBuffer(), 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::VisibleParticleIndices(), 0);
		glDepthMask(GL_TRUE);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_BLEND);
	}

} // namespace Boidsish
