#include "fire_effect_manager.h"

#include <algorithm>

#include "service_locator.h"
#include <numeric>
#include <queue>

#include "biome_properties.h"
#include "graphics.h" // For logger
#include "logger.h"
#include "model.h"
#include "profiler.h"
#include <GL/glew.h>
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
		if (grid_heads_buffer_ != 0) {
			glDeleteBuffers(1, &grid_heads_buffer_);
		}
		if (grid_next_buffer_ != 0) {
			glDeleteBuffers(1, &grid_next_buffer_);
		}
		if (dummy_vao_ != 0) {
			glDeleteVertexArrays(1, &dummy_vao_);
		}
	}

	void FireEffectManager::_EnsureShaderAndBuffers() {
		if (initialized_) {
			return;
		}

		// Create shaders
		lifecycle_shader_ = std::make_unique<ComputeShader>("shaders/fire_lifecycle.comp");
		if (!lifecycle_shader_->isValid()) {
			logger::ERROR("Failed to compile fire lifecycle shader - fire effects will be disabled");
			initialized_ = true; // Mark as initialized to prevent repeated attempts
			return;
		}

		behavior_shader_ = std::make_unique<ComputeShader>("shaders/fire_behavior.comp");
		if (!behavior_shader_->isValid()) {
			logger::ERROR("Failed to compile fire behavior shader - fire effects will be disabled");
			initialized_ = true; // Mark as initialized to prevent repeated attempts
			return;
		}

		fixup_shader_ = std::make_unique<ComputeShader>("shaders/particle_command_fixup.comp");
		if (!fixup_shader_->isValid()) {
			logger::ERROR("Failed to compile particle fixup shader - fire effects will be disabled");
			initialized_ = true; // Mark as initialized to prevent repeated attempts
			return;
		}

		grid_build_shader_ = std::make_unique<ComputeShader>("shaders/particle_grid_build.comp");
		if (!grid_build_shader_->isValid()) {
			logger::ERROR("Failed to compile particle grid build shader");
		}

		render_shader_ = std::make_unique<Shader>("shaders/fire.vert", "shaders/fire.frag");

		// Set up UBO bindings for the render shader
		render_shader_->use();
		render_shader_->bindUniformBlock("FrustumData", Constants::UboBinding::FrustumData());
		render_shader_->bindUniformBlock("Lighting", Constants::UboBinding::Lighting());
		render_shader_->bindUniformBlock("TemporalData", Constants::UboBinding::TemporalData());

		// Set up UBO bindings for the compute shaders
		auto setup_comp_ubos = [&](ComputeShader* shader) {
			shader->use();
			shader->bindUniformBlock("Lighting", Constants::UboBinding::Lighting());
			shader->bindUniformBlock("FrustumData", Constants::UboBinding::FrustumData());
			shader->bindUniformBlock("VisualEffects", Constants::UboBinding::VisualEffects());
		};

		setup_comp_ubos(lifecycle_shader_.get());
		setup_comp_ubos(behavior_shader_.get());

		// Create buffers
		glGenBuffers(1, &particle_buffer_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, particle_buffer_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxParticles * sizeof(Particle), nullptr, GL_DYNAMIC_DRAW);
		// Zero out the buffer to ensure lifetimes start at 0
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

		emitter_pb_ = std::make_unique<PersistentBuffer<Emitter>>(GL_SHADER_STORAGE_BUFFER, kMaxEmitters, 3);
		indirection_pb_ = std::make_unique<PersistentBuffer<int>>(GL_SHADER_STORAGE_BUFFER, kMaxParticles, 3);
		terrain_chunk_pb_ = std::make_unique<PersistentBuffer<glm::vec4>>(GL_SHADER_STORAGE_BUFFER, 8192, 3);
		slice_data_pb_ = std::make_unique<PersistentBuffer<glm::vec4>>(GL_SHADER_STORAGE_BUFFER, kMaxEmitters * 64, 3);

		glGenBuffers(1, &visible_indices_buffer_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, visible_indices_buffer_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxParticles * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);

		glGenBuffers(1, &live_indices_buffer_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, live_indices_buffer_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxParticles * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);

		draw_command_pb_ = std::make_unique<PersistentBuffer<DrawArraysIndirectCommand>>(GL_SHADER_STORAGE_BUFFER, 1, 3);
		behavior_command_pb_ = std::make_unique<PersistentBuffer<uint32_t>>(GL_SHADER_STORAGE_BUFFER, 4, 3);

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

		particle_to_emitter_map_.resize(kMaxParticles, -1);

		// A dummy VAO is required by OpenGL 4.3 core profile for drawing arrays.
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
		std::lock_guard<std::mutex> lock(mutex_);
		_EnsureShaderAndBuffers();

		// If compute shaders failed, fire effects are disabled
		if (!lifecycle_shader_ || !lifecycle_shader_->isValid() || !behavior_shader_ || !behavior_shader_->isValid() ||
		    !fixup_shader_ || !fixup_shader_->isValid()) {
			return nullptr;
		}

		// Find an inactive slot to reuse
		for (size_t i = 0; i < effects_.size(); ++i) {
			if (!effects_[i]) {
				effects_[i] = std::make_shared<
					FireEffect>(position, style, direction, velocity, max_particles, lifetime, type, dimensions, sweep);
				needs_reallocation_ = true;
				return effects_[i];
			}
		}

		// If no inactive slots, add a new one if under capacity
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
			std::lock_guard<std::mutex> lock(mutex_);
			for (size_t i = 0; i < effects_.size(); ++i) {
				if (effects_[i] == effect) {
					effects_[i] = nullptr; // Mark as inactive
					needs_reallocation_ = true;
					return;
				}
			}
		}
	}

	void FireEffectManager::UpdateCPU(
		float                         delta_time,
		float                         time,
		float                         ambient_density,
		const std::vector<glm::vec4>& chunk_info
	) {
		PROJECT_PROFILE_SCOPE("FireEffectManager::UpdateCPU");
		std::lock_guard<std::mutex> lock(mutex_);
		if (!initialized_)
			return;

		time_ = time;
		if (std::abs(ambient_density - ambient_density_) > 0.01f) {
			ambient_density_ = ambient_density;
			needs_reallocation_ = true;
		}

		// --- Effect Lifetime Management ---
		for (auto& effect : effects_) {
			if (effect) {
				float lifetime = effect->GetLifetime();
				if (lifetime > 0.0f) {
					float lived = effect->GetLived();
					lived += delta_time;
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

		// Advance frames for persistent buffers
		emitter_pb_->AdvanceFrame();
		indirection_pb_->AdvanceFrame();
		terrain_chunk_pb_->AdvanceFrame();
		slice_data_pb_->AdvanceFrame();
		draw_command_pb_->AdvanceFrame();
		behavior_command_pb_->AdvanceFrame();

		// --- Update Emitters and Slice Data ---
		Emitter* emitters_ptr = emitter_pb_->GetFrameDataPtr();
		glm::vec4* slice_data_ptr = slice_data_pb_->GetFrameDataPtr();
		int slice_points_count = 0;

		for (size_t i = 0; i < effects_.size(); ++i) {
			auto& effect = effects_[i];
			if (effect) {
				emitters_ptr[i] = {
					effect->GetPosition(),
					(int)effect->GetStyle(),
					effect->GetDirection(),
					effect->IsActive() ? 1 : 0,
					effect->GetVelocity(),
					effect->GetId(),
					effect->GetDimensions(),
					(int)effect->GetType(),
					effect->GetSweep(),
					0, 0, 0, 0.0f, // slice data fields
					effect->NeedsClear() ? 1 : 0,
					{0, 0}
				};

				auto model = effect->GetSourceModel();
				if (effect->GetType() == EmitterType::Model && model) {
					ModelSlice slice = model->GetSlice(effect->GetDirection(), effect->GetSweep());
					emitters_ptr[i].slice_area = slice.area;
					if (!slice.triangles.empty() && slice_points_count + 64 <= (int)slice_data_pb_->GetElementCount()) {
						emitters_ptr[i].use_slice_data = 1;
						emitters_ptr[i].slice_data_offset = slice_points_count;
						emitters_ptr[i].slice_data_count = 64;

						for (int j = 0; j < 64; ++j) {
							slice_data_ptr[slice_points_count++] = glm::vec4(slice.GetRandomPoint(), 1.0f);
						}
					}
				}

				if (effect->NeedsClear()) {
					effect->ResetClearRequest();
				}
			} else {
				emitters_ptr[i] = {glm::vec3(0), 0, glm::vec3(0), 0, glm::vec3(0), 0, glm::vec3(0), 0, 0.0f, 0, 0, 0, 0.0f, 0, {0, 0}};
			}
		}

		// Update Indirection
		std::memcpy(indirection_pb_->GetFrameDataPtr(), particle_to_emitter_map_.data(), kMaxParticles * sizeof(int));

		// Update Terrain Chunk Info
		int num_chunks = std::min((int)chunk_info.size(), (int)terrain_chunk_pb_->GetElementCount());
		if (num_chunks > 0) {
			std::memcpy(terrain_chunk_pb_->GetFrameDataPtr(), chunk_info.data(), num_chunks * sizeof(glm::vec4));
		}
		num_active_chunks_ = num_chunks;

		// Reset draw and behavior commands
		DrawArraysIndirectCommand* draw_cmd = draw_command_pb_->GetFrameDataPtr();
		draw_cmd->count = 0;
		draw_cmd->instanceCount = 1;
		draw_cmd->first = 0;
		draw_cmd->baseInstance = 0;

		uint32_t* behavior_cmd = behavior_command_pb_->GetFrameDataPtr();
		behavior_cmd[0] = 0; // num_groups_x
		behavior_cmd[1] = 1;
		behavior_cmd[2] = 1;
		behavior_cmd[3] = 0; // count (used by fixup)
	}

	void FireEffectManager::UpdateGPU(
		float      delta_time,
		GLuint     heightmap_texture,
		GLuint     curl_noise_texture,
		GLuint     biome_texture,
		GLuint     lighting_ubo,
		GLintptr   lighting_ubo_offset,
		GLsizeiptr lighting_ubo_size,
		GLuint     frustum_ubo,
		GLintptr   frustum_offset,
		GLuint     extra_noise_texture,
		GLuint     visual_effects_ubo,
		GLintptr   vfx_offset,
		GLsizeiptr vfx_size
	) {
		PROJECT_PROFILE_SCOPE("FireEffectManager::UpdateGPU");
		std::lock_guard<std::mutex> lock(mutex_);
		if (!initialized_ || !lifecycle_shader_ || !lifecycle_shader_->isValid() || !behavior_shader_ ||
		    !behavior_shader_->isValid() || !fixup_shader_ || !fixup_shader_->isValid()) {
			return;
		}

		// --- Bindings ---
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleBuffer(), particle_buffer_);
		emitter_pb_->BindRange(Constants::SsboBinding::EmitterBuffer());
		indirection_pb_->BindRange(Constants::SsboBinding::IndirectionBuffer());
		terrain_chunk_pb_->BindRange(Constants::SsboBinding::TerrainChunkInfo());
		slice_data_pb_->BindRange(Constants::SsboBinding::SliceData());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::VisibleParticleIndices(), visible_indices_buffer_);
		draw_command_pb_->BindRange(Constants::SsboBinding::ParticleDrawCommand());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::LiveParticleIndices(), live_indices_buffer_);
		behavior_command_pb_->BindRange(Constants::SsboBinding::BehaviorDrawCommand());

		auto bind_textures_and_uniforms = [&](ComputeShader* shader) {
			shader->use();
			shader->setFloat("u_delta_time", delta_time);
			shader->setFloat("u_time", time_);
			shader->setFloat("u_ambient_density", ambient_density_);
			shader->setInt("u_num_emitters", effects_.size());
			shader->setInt("u_num_chunks", num_active_chunks_);
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
			glBindBufferRange(GL_UNIFORM_BUFFER, Constants::UboBinding::Lighting(), lighting_ubo, lighting_ubo_offset, lighting_ubo_size);
		}
		if (frustum_ubo != 0) {
			glBindBufferRange(GL_UNIFORM_BUFFER, Constants::UboBinding::FrustumData(), frustum_ubo, frustum_offset, sizeof(FrustumDataGPU));
		}
		if (visual_effects_ubo != 0) {
			glBindBufferRange(GL_UNIFORM_BUFFER, Constants::UboBinding::VisualEffects(), visual_effects_ubo, vfx_offset, vfx_size);
		}

		// Phase 1: Lifecycle
		bind_textures_and_uniforms(lifecycle_shader_.get());
		glDispatchCompute((kMaxParticles / Constants::Class::Particles::ComputeGroupSize()) + 1, 1, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		// Phase 2: Build Spatial Grid
		if (grid_build_shader_ && grid_build_shader_->isValid()) {
			grid_build_shader_->use();
			grid_build_shader_->setUint("u_grid_size", Constants::Class::Particles::ParticleGridSize());
			grid_build_shader_->setFloat("u_cell_size", Constants::Class::Particles::ParticleGridCellSize());
			grid_build_shader_->setInt("u_num_particles", kMaxParticles);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleBuffer(), particle_buffer_);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleGridHeads(), grid_heads_buffer_);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleGridNext(), grid_next_buffer_);

			grid_build_shader_->setInt("u_mode", 0); // Clear
			glDispatchCompute((Constants::Class::Particles::ParticleGridSize() / Constants::Class::Particles::ComputeGroupSize()) + 1, 1, 1);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

			grid_build_shader_->setInt("u_mode", 1); // Build
			glDispatchCompute((kMaxParticles / Constants::Class::Particles::ComputeGroupSize()) + 1, 1, 1);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		}

		// Phase 3: Command Fixup
		fixup_shader_->use();
		behavior_command_pb_->BindRange(Constants::SsboBinding::BehaviorDrawCommand());
		glDispatchCompute(1, 1, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

		// Phase 4: Behavior
		bind_textures_and_uniforms(behavior_shader_.get());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleGridHeads(), grid_heads_buffer_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleGridNext(), grid_next_buffer_);
		glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, behavior_command_pb_->GetBufferId());
		glDispatchComputeIndirect(static_cast<GLintptr>(behavior_command_pb_->GetFrameOffset()));
		glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, 0);

		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleBuffer(), 0);
	}

	void FireEffectManager::_UpdateParticleAllocation() {
		// --- 1. Calculate Ideal Distribution ---
		std::vector<int> ideal_counts(effects_.size(), 0);
		int              total_particle_demand = 0;
		int              num_unlimited_emitters = 0;
		int              num_active_emitters = 0;

		for (const auto& effect : effects_) {
			if (effect) {
				num_active_emitters++;
				int max_p = effect->GetMaxParticles();
				if (max_p != -1) {
					total_particle_demand += max_p;
				} else {
					num_unlimited_emitters++;
				}
			}
		}

		int avg_particles_per_unlimited = 0;
		// Maintain a 5% safety margin so ambient particles always have some space
		int fire_budget = static_cast<int>(kMaxParticles * (1.0f - ambient_density_ - 0.05f));
		fire_budget = std::clamp(fire_budget, 0, kMaxParticles);

		if (num_unlimited_emitters > 0) {
			int available_for_unlimited = fire_budget - total_particle_demand;
			if (available_for_unlimited > 0) {
				avg_particles_per_unlimited = available_for_unlimited / num_unlimited_emitters;
			}
		}

		for (size_t i = 0; i < effects_.size(); ++i) {
			if (effects_[i]) {
				int max_p = effects_[i]->GetMaxParticles();
				if (max_p != -1) {
					ideal_counts[i] = max_p;
				} else {
					ideal_counts[i] = avg_particles_per_unlimited;
				}
			}
		}

		// Distribute any remainder due to integer division
		int current_total = std::accumulate(ideal_counts.begin(), ideal_counts.end(), 0);
		int remainder = (num_active_emitters > 0) ? (fire_budget - current_total) : 0;
		for (size_t i = 0; i < effects_.size() && remainder > 0; ++i) {
			if (effects_[i]) {
				ideal_counts[i]++;
				remainder--;
			}
		}

		// --- 2. Calculate Current Distribution ---
		std::vector<int>              current_counts(effects_.size(), 0);
		std::vector<int>              general_nulls;
		std::vector<int>              reserved_nulls;
		std::vector<std::vector<int>> particles_by_emitter(effects_.size());

		int reserved_start = fire_budget;

		// Iterate backwards to prioritize lower indices for fire emitters when taking from null lists
		for (int i = kMaxParticles - 1; i >= 0; --i) {
			int emitter_index = particle_to_emitter_map_[i];
			if (emitter_index != -1 && emitter_index < (int)effects_.size() && effects_[emitter_index]) {
				current_counts[emitter_index]++;
				particles_by_emitter[emitter_index].push_back(i);
			} else {
				particle_to_emitter_map_[i] = -1; // Explicitly return to ambient pool
				if (i < reserved_start) {
					general_nulls.push_back(i);
				} else {
					reserved_nulls.push_back(i);
				}
			}
		}

		// --- 3. Identify Over/Under Budget Emitters ---
		std::vector<int>                           to_reclaim; // Particle indices to take from over-budget emitters
		std::priority_queue<std::pair<float, int>> to_fill;    // {need, index} for under-budget emitters

		for (size_t i = 0; i < effects_.size(); ++i) {
			int diff = ideal_counts[i] - current_counts[i];
			if (diff > 0) {
				float need = (float)diff / ideal_counts[i];
				to_fill.push({need, (int)i});
			} else if (diff < 0) {
				// Reclaim -diff particles from this emitter efficiently using pre-collected indices
				int num_to_reclaim = -diff;
				for (int j = 0; j < num_to_reclaim; ++j) {
					if (!particles_by_emitter[i].empty()) {
						int particle_index = particles_by_emitter[i].back();
						to_reclaim.push_back(particle_index);
						particles_by_emitter[i].pop_back();
						particle_to_emitter_map_[particle_index] = -1; // Explicitly return to ambient pool
					}
				}
			}
		}

		// --- 4. Perform Stable Re-mapping ---
		// First, use general null particles to fill under-budget emitters
		while (!to_fill.empty() && !general_nulls.empty()) {
			int emitter_index = to_fill.top().second;
			to_fill.pop();
			int particle_index = general_nulls.back(); // Lowest available index due to backward loop
			general_nulls.pop_back();

			particle_to_emitter_map_[particle_index] = emitter_index;
			ideal_counts[emitter_index]--;
			if (ideal_counts[emitter_index] > current_counts[emitter_index]) {
				float need = (float)(ideal_counts[emitter_index] - current_counts[emitter_index]) /
					ideal_counts[emitter_index];
				to_fill.push({need, emitter_index});
			}
		}

		// Second, use reserved null particles if still needed
		while (!to_fill.empty() && !reserved_nulls.empty()) {
			int emitter_index = to_fill.top().second;
			to_fill.pop();
			int particle_index = reserved_nulls.back();
			reserved_nulls.pop_back();

			particle_to_emitter_map_[particle_index] = emitter_index;
			ideal_counts[emitter_index]--;
			if (ideal_counts[emitter_index] > current_counts[emitter_index]) {
				float need = (float)(ideal_counts[emitter_index] - current_counts[emitter_index]) /
					ideal_counts[emitter_index];
				to_fill.push({need, emitter_index});
			}
		}

		// Third, use reclaimed particles from over-budget emitters if still needed
		while (!to_fill.empty() && !to_reclaim.empty()) {
			int emitter_index = to_fill.top().second;
			to_fill.pop();
			int particle_index = to_reclaim.back();
			to_reclaim.pop_back();

			particle_to_emitter_map_[particle_index] = emitter_index;
			ideal_counts[emitter_index]--;
			if (ideal_counts[emitter_index] > current_counts[emitter_index]) {
				float need = (float)(ideal_counts[emitter_index] - current_counts[emitter_index]) /
					ideal_counts[emitter_index];
				to_fill.push({need, emitter_index});
			}
		}
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
		if (!initialized_ || !lifecycle_shader_ || !lifecycle_shader_->isValid() || !behavior_shader_ ||
		    !behavior_shader_->isValid() || !fixup_shader_ || !fixup_shader_->isValid()) {
			return;
		}

		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // Additive blending for fire
		glDepthMask(GL_FALSE);                       // Disable depth writing
		glEnable(GL_PROGRAM_POINT_SIZE);

		render_shader_->use();
		render_shader_->setMat4("u_view", view);
		render_shader_->setMat4("u_projection", projection);
		render_shader_->setVec3("u_camera_pos", camera_pos);
		render_shader_->setFloat("u_time", time_);

		// Pass biome albedos for biased ambient particle colors
		for (int i = 0; i < static_cast<int>(Biome::Count); ++i) {
			render_shader_->setVec3("u_biomeAlbedos[" + std::to_string(i) + "]", kBiomes[i].albedo);
		}

		// Bind Lighting UBO for nightFactor
		render_shader_->bindUniformBlock("Lighting", Constants::UboBinding::Lighting());

		emitter_pb_->BindRange(Constants::SsboBinding::EmitterBuffer());
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

		// Enable GPU frustum culling for particles
		render_shader_->setBool("enableFrustumCulling", true);
		render_shader_->setFloat("frustumCullRadius", 2.0f); // Particle cull radius

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleBuffer(), particle_buffer_);

		// We don't have a VAO for the particles since we generate them in the shader.
		// We use indirect rendering to draw only visible particles.
		// A dummy VAO is required by OpenGL 4.3 core profile.
		glBindVertexArray(dummy_vao_);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, draw_command_pb_->GetBufferId());
		glDrawArraysIndirect(GL_POINTS, (void*)(uintptr_t)draw_command_pb_->GetFrameOffset());
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
		glBindVertexArray(0);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleBuffer(), 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::EmitterBuffer(), 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::VisibleParticleIndices(), 0);

		glDepthMask(GL_TRUE);                              // Re-enable depth writing
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Reset blend mode
		glDisable(GL_BLEND);
	}

} // namespace Boidsish
