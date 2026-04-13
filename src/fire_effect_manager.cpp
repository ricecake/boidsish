#include "fire_effect_manager.h"

#include <algorithm>
#include <iostream>
#include <random>
#include <vector>

#include "constants.h"
#include "logger.h"
#include "profiler.h"
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Boidsish {

	// This must match the struct in the compute shader
	struct Particle {
		glm::vec4 pos;
		glm::vec4 vel;
		alignas(16) glm::vec3 epicenter;
		int   style;
		int   emitter_index;
		int   emitter_id;
		float extras[2];
	};

    const int FireEffectManager::kMaxParticles;
    const int FireEffectManager::kMaxEmitters;

	FireEffectManager::FireEffectManager() {}

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
		if (grid_heads_buffer_ != 0) {
			glDeleteBuffers(1, &grid_heads_buffer_);
		}
		if (grid_next_buffer_ != 0) {
			glDeleteBuffers(1, &grid_next_buffer_);
		}
		if (emitter_buffer_ != 0) {
			glDeleteBuffers(1, &emitter_buffer_);
		}
		if (indirection_buffer_ != 0) {
			glDeleteBuffers(1, &indirection_buffer_);
		}
		if (terrain_chunk_buffer_ != 0) {
			glDeleteBuffers(1, &terrain_chunk_buffer_);
		}
		if (slice_data_buffer_ != 0) {
			glDeleteBuffers(1, &slice_data_buffer_);
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
		if (dummy_vao_ != 0) {
			glDeleteVertexArrays(1, &dummy_vao_);
		}
	}

	void FireEffectManager::_EnsureShaderAndBuffers() {
		if (initialized_)
			return;

		lifecycle_shader_ = std::make_unique<ComputeShader>("shaders/fire_lifecycle.comp");
		behavior_shader_ = std::make_unique<ComputeShader>("shaders/fire_behavior.comp");
		fixup_shader_ = std::make_unique<ComputeShader>("shaders/fire_fixup.comp");
		grid_build_shader_ = std::make_unique<ComputeShader>("shaders/fire_grid_build.comp");
		render_shader_ = std::make_unique<Shader>("shaders/fire.vert", "shaders/fire.frag");

		if (!lifecycle_shader_->isValid() || !behavior_shader_->isValid() || !fixup_shader_->isValid() ||
		    !grid_build_shader_->isValid() || !render_shader_->ID) {
			logger::ERROR("Failed to compile fire effect shaders");
			return;
		}

		glGenBuffers(1, &particle_buffer_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, particle_buffer_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxParticles * sizeof(Particle), nullptr, GL_DYNAMIC_DRAW);

		glGenBuffers(1, &grid_heads_buffer_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, grid_heads_buffer_);
		uint32_t gridSize = Constants::Class::Particles::ParticleGridSize();
		glBufferData(
			GL_SHADER_STORAGE_BUFFER,
			gridSize * gridSize * gridSize * sizeof(int),
			nullptr,
			GL_DYNAMIC_DRAW
		);

		glGenBuffers(1, &grid_next_buffer_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, grid_next_buffer_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxParticles * sizeof(int), nullptr, GL_DYNAMIC_DRAW);

		glGenBuffers(1, &emitter_buffer_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, emitter_buffer_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxEmitters * sizeof(Emitter), nullptr, GL_DYNAMIC_DRAW);
		emitter_buffer_capacity_ = kMaxEmitters;

		glGenBuffers(1, &indirection_buffer_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, indirection_buffer_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxParticles * sizeof(int), nullptr, GL_DYNAMIC_DRAW);

		glGenBuffers(1, &terrain_chunk_buffer_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, terrain_chunk_buffer_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, 1024 * sizeof(glm::vec4), nullptr, GL_DYNAMIC_DRAW);

		glGenBuffers(1, &slice_data_buffer_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, slice_data_buffer_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxEmitters * 64 * sizeof(glm::vec4), nullptr, GL_DYNAMIC_DRAW);

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

		glGenVertexArrays(1, &dummy_vao_);

		render_shader_->use();
		GLuint lighting_idx = glGetUniformBlockIndex(render_shader_->ID, "Lighting");
		if (lighting_idx != GL_INVALID_INDEX) {
			glUniformBlockBinding(render_shader_->ID, lighting_idx, Constants::UboBinding::Lighting());
		}

		auto setup_comp_lighting = [&](ComputeShader* shader) {
			shader->use();
			GLuint comp_lighting_idx = glGetUniformBlockIndex(shader->ID, "Lighting");
			if (comp_lighting_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(shader->ID, comp_lighting_idx, Constants::UboBinding::Lighting());
			}
		};
		setup_comp_lighting(lifecycle_shader_.get());
		setup_comp_lighting(behavior_shader_.get());

		initialized_ = true;
	}

	void FireEffectManager::_UpdateParticleAllocation() {
		std::vector<std::shared_ptr<FireEffect>> active_effects;
		for (auto& effect : effects_) {
			if (effect) {
				active_effects.push_back(effect);
			}
		}
		effects_ = active_effects;

		size_t total_required = 0;
		for (const auto& effect : effects_) {
			total_required += effect->GetMaxParticles();
		}

		if (total_required > kMaxParticles) {
			logger::WARNING("FireEffectManager: Particle limit reached ({}/{})", total_required, kMaxParticles);
		}

		particle_to_emitter_map_.clear();
		particle_to_emitter_map_.reserve(kMaxParticles);

		int emitter_idx = 0;
		for (const auto& effect : effects_) {
			int count = std::min(effect->GetMaxParticles(), kMaxParticles - (int)particle_to_emitter_map_.size());
			for (int i = 0; i < count; ++i) {
				particle_to_emitter_map_.push_back(emitter_idx);
			}
			emitter_idx++;
		}

		// Re-allocate indirection buffer if necessary (rare)
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, indirection_buffer_);
		glBufferSubData(
			GL_SHADER_STORAGE_BUFFER,
			0,
			particle_to_emitter_map_.size() * sizeof(int),
			particle_to_emitter_map_.data()
		);
	}

	void FireEffectManager::Update(
		const GlobalRenderState&      render_state,
		const std::vector<glm::vec4>& chunk_info,
		GLuint                        heightmap_texture,
		GLuint                        curl_noise_texture,
		GLuint                        biome_texture,
		GLuint                        extra_noise_texture
	) {
		PROJECT_PROFILE_SCOPE("FireEffectManager::Update");
		std::lock_guard<std::mutex> lock(mutex_);
		if (!initialized_ || !lifecycle_shader_ || !lifecycle_shader_->isValid() || !behavior_shader_ ||
		    !behavior_shader_->isValid() || !fixup_shader_ || !fixup_shader_->isValid()) {
			return;
		}

		time_ = render_state.time;
		// --- Effect Lifetime Management ---
		for (auto& effect : effects_) {
			if (effect) {
				float lifetime = effect->GetLifetime();
				if (lifetime > 0.0f) {
					float lived = effect->GetLived();
					lived += render_state.delta_time;
					effect->SetLived(lived);
					if (lived >= lifetime) {
						effect = nullptr; // Mark for removal
						needs_reallocation_ = true;
					}
				}
			}
		}

		if (needs_reallocation_) {
			_UpdateParticleAllocation();
			needs_reallocation_ = false;
		}

		// --- Update Emitters and Slice Data ---
		std::vector<Emitter> emitters;

		for (const auto& effect : effects_) {
			if (!effect)
				continue;

			Emitter e{};
			e.position = effect->GetPosition();
			e.style = (int)effect->GetStyle();
			e.direction = effect->GetDirection();
			e.is_active = effect->IsActive() ? 1 : 0;
			e.velocity = effect->GetVelocity();
			e.id = effect->GetId();
			e.dimensions = effect->GetDimensions();
			e.type = (int)effect->GetType();
			e.sweep = effect->GetSweep();

			if (effect->NeedsClear()) {
				e.request_clear = 1;
				effect->ResetClearRequest();
			}

			emitters.push_back(e);
		}

		if (emitters.empty() && render_state.ambient_particle_density <= 0.001f)
			return;

		// Resize emitter buffer if needed
		if (emitters.size() > emitter_buffer_capacity_) {
			size_t new_capacity = emitters.size() + 10;
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, emitter_buffer_);
			glBufferData(GL_SHADER_STORAGE_BUFFER, new_capacity * sizeof(Emitter), nullptr, GL_DYNAMIC_DRAW);
			emitter_buffer_capacity_ = new_capacity;
		}

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, emitter_buffer_);
		if (!emitters.empty()) {
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, emitters.size() * sizeof(Emitter), emitters.data());
		}

		if (!chunk_info.empty()) {
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, terrain_chunk_buffer_);
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, chunk_info.size() * sizeof(glm::vec4), chunk_info.data());
		}

		// Reset draw commands
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, draw_command_buffer_);
		uint32_t draw_cmd_init[4] = {0, 1, 0, 0}; // count, instanceCount, first, baseInstance
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(draw_cmd_init), draw_cmd_init);

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, behavior_command_buffer_);
		uint32_t behavior_cmd_init[4] = {0, 1, 1, 0}; // num_groups_x, num_groups_y, num_groups_z, count
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(behavior_cmd_init), behavior_cmd_init);

		// --- Common Bindings ---
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 16, particle_buffer_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 22, emitter_buffer_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 17, indirection_buffer_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 23, terrain_chunk_buffer_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 24, slice_data_buffer_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 27, visible_indices_buffer_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 28, draw_command_buffer_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 33, live_indices_buffer_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 34, behavior_command_buffer_);

		auto bind_textures_and_uniforms = [&](ComputeShader* shader) {
			shader->use();
			shader->setFloat("u_delta_time", render_state.delta_time);
			shader->setFloat("u_time", time_);
			shader->setFloat("u_ambient_density", render_state.ambient_particle_density);
			shader->setInt("u_num_emitters", emitters.size());
			shader->setInt("u_num_chunks", static_cast<int>(chunk_info.size()));
			shader->setUint("u_grid_size", Constants::Class::Particles::ParticleGridSize());
			shader->setFloat("u_cell_size", Constants::Class::Particles::ParticleGridCellSize());

			if (heightmap_texture != 0) {
				glActiveTexture(GL_TEXTURE7);
				glBindTexture(GL_TEXTURE_2D_ARRAY, heightmap_texture);
				shader->setInt("u_heightmapArray", 7);
			}

			if (curl_noise_texture != 0) {
				glActiveTexture(GL_TEXTURE6);
				glBindTexture(GL_TEXTURE_3D, curl_noise_texture);
				shader->setInt("u_curlTexture", 6);
			}

			if (biome_texture != 0) {
				glActiveTexture(GL_TEXTURE8);
				glBindTexture(GL_TEXTURE_2D_ARRAY, biome_texture);
				shader->setInt("u_biomeMap", 8);
			}

			if (extra_noise_texture != 0) {
				glActiveTexture(GL_TEXTURE9);
				glBindTexture(GL_TEXTURE_3D, extra_noise_texture);
				shader->setInt("u_extraNoiseTexture", 9);
			}
		};

		render_state.BindLighting(Constants::UboBinding::Lighting());
		render_state.BindFrustum(Constants::UboBinding::FrustumData());

		// --- Phase 1: Lifecycle ---
		bind_textures_and_uniforms(lifecycle_shader_.get());
		glDispatchCompute((kMaxParticles + 63) / 64, 1, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

		// --- Phase 2: Behavior (Indirect) ---
		bind_textures_and_uniforms(behavior_shader_.get());
		glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, behavior_command_buffer_);
		glDispatchComputeIndirect(0);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

		// --- Phase 3: Fixup ---
		bind_textures_and_uniforms(fixup_shader_.get());
		glDispatchCompute((kMaxParticles + 63) / 64, 1, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

		glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, 0);
	}

	void FireEffectManager::Render(
		const glm::mat4& view,
		const glm::mat4& projection,
		const glm::vec3& camera_pos,
		GLuint           noise_texture,
		GLuint           extra_noise_texture
	) {
		PROJECT_PROFILE_SCOPE("FireEffectManager::Render");
		std::lock_guard<std::mutex> lock(mutex_);
		if (!initialized_ || !render_shader_ || !render_shader_->ID) {
			return;
		}

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		glDepthMask(GL_FALSE);

		render_shader_->use();
		render_shader_->setMat4("view", view);
		render_shader_->setMat4("projection", projection);
		render_shader_->setVec3("cameraPos", camera_pos);
		render_shader_->setFloat("time", time_);

		if (noise_texture != 0) {
			glActiveTexture(GL_TEXTURE6);
			glBindTexture(GL_TEXTURE_3D, noise_texture);
			render_shader_->setInt("u_curlTexture", 6);
		}

		if (extra_noise_texture != 0) {
			glActiveTexture(GL_TEXTURE9);
			glBindTexture(GL_TEXTURE_3D, extra_noise_texture);
			render_shader_->setInt("u_extraNoiseTexture", 9);
		}

		glBindVertexArray(dummy_vao_);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, draw_command_buffer_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 16, particle_buffer_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 27, visible_indices_buffer_);

		glDrawArraysIndirect(GL_POINTS, 0);

		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
		glBindVertexArray(0);
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
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
		std::lock_guard<std::mutex> lock(mutex_);
		auto                        effect = std::make_shared<FireEffect>(
            position,
            style,
            direction,
            velocity,
            max_particles,
            lifetime,
            type,
            dimensions,
            sweep
        );
		effects_.push_back(effect);
		needs_reallocation_ = true;
		return effect;
	}

	void FireEffectManager::RemoveEffect(const std::shared_ptr<FireEffect>& effect) {
		std::lock_guard<std::mutex> lock(mutex_);
		auto                        it = std::find(effects_.begin(), effects_.end(), effect);
		if (it != effects_.end()) {
			*it = nullptr;
			needs_reallocation_ = true;
		}
	}

} // namespace Boidsish
