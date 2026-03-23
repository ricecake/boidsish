#include "fire_effect_manager.h"

#include <algorithm>
#include <numeric>
#include <queue>

#include "biome_properties.h"
#include "graphics.h" // For logger
#include "logger.h"
#include "model.h"
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

	FireEffectManager::FireEffectManager(int num_voxel_buffers) {
		for (int i = 0; i < num_voxel_buffers; ++i) {
			voxel_managers_.push_back(std::make_unique<VoxelBrickManager>());
		}
	}

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
		GLuint frustum_idx = glGetUniformBlockIndex(render_shader_->ID, "FrustumData");
		if (frustum_idx != GL_INVALID_INDEX) {
			glUniformBlockBinding(render_shader_->ID, frustum_idx, Constants::UboBinding::FrustumData());
		}
		GLuint lighting_idx = glGetUniformBlockIndex(render_shader_->ID, "Lighting");
		if (lighting_idx != GL_INVALID_INDEX) {
			glUniformBlockBinding(render_shader_->ID, lighting_idx, Constants::UboBinding::Lighting());
		}
		GLuint temporal_idx = glGetUniformBlockIndex(render_shader_->ID, "TemporalData");
		if (temporal_idx != GL_INVALID_INDEX) {
			glUniformBlockBinding(render_shader_->ID, temporal_idx, Constants::UboBinding::TemporalData());
		}

		// Set up UBO bindings for the compute shaders
		auto setup_comp_ubos = [&](ComputeShader* shader) {
			shader->use();
			GLuint comp_lighting_idx = glGetUniformBlockIndex(shader->ID, "Lighting");
			if (comp_lighting_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(shader->ID, comp_lighting_idx, Constants::UboBinding::Lighting());
			}
			GLuint comp_frustum_idx = glGetUniformBlockIndex(shader->ID, "FrustumData");
			if (comp_frustum_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(shader->ID, comp_frustum_idx, Constants::UboBinding::FrustumData());
			}
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

		glGenBuffers(1, &emitter_buffer_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, emitter_buffer_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxEmitters * sizeof(Emitter), nullptr, GL_DYNAMIC_DRAW);
		emitter_buffer_capacity_ = kMaxEmitters;

		glGenBuffers(1, &indirection_buffer_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, indirection_buffer_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxParticles * sizeof(int), nullptr, GL_DYNAMIC_DRAW);

		glGenBuffers(1, &terrain_chunk_buffer_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, terrain_chunk_buffer_);
		// Pre-allocate for 1024 chunks as a reasonable default
		glBufferData(GL_SHADER_STORAGE_BUFFER, 1024 * sizeof(glm::vec4), nullptr, GL_DYNAMIC_DRAW);

		glGenBuffers(1, &slice_data_buffer_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, slice_data_buffer_);
		// Max emitters (64) * 64 points per slice as a default
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
		// DispatchIndirectCommand: 4 * uint32_t (num_groups_x, y, z, and count)
		glBufferData(GL_DRAW_INDIRECT_BUFFER, 4 * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

		particle_to_emitter_map_.resize(kMaxParticles, -1);

		// A dummy VAO is required by OpenGL 4.3 core profile for drawing arrays.
		glGenVertexArrays(1, &dummy_vao_);

		for (auto& vm : voxel_managers_) {
			vm->Initialize();
		}

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

	void FireEffectManager::Update(
		float                         delta_time,
		float                         time,
		float                         ambient_density,
		const std::vector<glm::vec4>& chunk_info,
		GLuint                        heightmap_texture,
		GLuint                        curl_noise_texture,
		GLuint                        biome_texture,
		GLuint                        lighting_ubo,
		GLuint                        frustum_ubo,
		GLintptr                      frustum_offset,
		GLuint                        extra_noise_texture
	) {
		std::lock_guard<std::mutex> lock(mutex_);
		if (!initialized_ || !lifecycle_shader_ || !lifecycle_shader_->isValid() || !behavior_shader_ ||
		    !behavior_shader_->isValid() || !fixup_shader_ || !fixup_shader_->isValid()) {
			return;
		}

		time_ = time;
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

		// --- Update Emitters and Slice Data ---
		std::vector<Emitter>   emitters;
		std::vector<glm::vec4> slice_points;
		emitters.reserve(effects_.size());

		for (const auto& effect : effects_) {
			if (effect) {
				Emitter emitter = {
					effect->GetPosition(),
					(int)effect->GetStyle(),
					effect->GetDirection(),
					effect->IsActive() ? 1 : 0, // is_active
					effect->GetVelocity(),
					effect->GetId(),
					effect->GetDimensions(),
					(int)effect->GetType(),
					effect->GetSweep(),
					0,    // use_slice_data
					0,    // slice_data_offset
					0,    // slice_data_count
					0.0f, // slice_area
					effect->NeedsClear() ? 1 : 0,
					{0, 0} // padding
				};

				auto model = effect->GetSourceModel();
				if (effect->GetType() == EmitterType::Model && model) {
					ModelSlice slice = model->GetSlice(effect->GetDirection(), effect->GetSweep());
					emitter.slice_area = slice.area;
					if (!slice.triangles.empty()) {
						emitter.use_slice_data = 1;
						emitter.slice_data_offset = static_cast<int>(slice_points.size());
						emitter.slice_data_count = 64; // Sample 64 points per slice

						for (int i = 0; i < emitter.slice_data_count; ++i) {
							slice_points.push_back(glm::vec4(slice.GetRandomPoint(), 1.0f));
						}
					}
				}

				if (effect->NeedsClear()) {
					effect->ResetClearRequest();
				}

				emitters.push_back(emitter);
			} else {
				// Add a placeholder for inactive emitters to maintain indexing
				emitters.push_back(
					{glm::vec3(0), 0, glm::vec3(0), 0, glm::vec3(0), 0, glm::vec3(0), 0, 0.0f, 0, 0, 0, 0.0f, 0, {0, 0}}
				);
			}
		}

		if (!slice_points.empty()) {
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, slice_data_buffer_);
			// Ensure buffer is large enough
			size_t required_size = slice_points.size() * sizeof(glm::vec4);
			GLint  current_size = 0;
			glGetBufferParameteriv(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &current_size);
			if (required_size > (size_t)current_size) {
				glBufferData(GL_SHADER_STORAGE_BUFFER, required_size * 2, nullptr, GL_DYNAMIC_DRAW);
			}
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, required_size, slice_points.data());
		}

		// Update emitter buffer, only reallocating if capacity exceeded
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, emitter_buffer_);
		if (!emitters.empty() && emitters.size() > emitter_buffer_capacity_) {
			// Grow buffer with headroom to avoid frequent reallocations
			size_t new_capacity = std::max(emitters.size() * 2, emitter_buffer_capacity_);
			glBufferData(GL_SHADER_STORAGE_BUFFER, new_capacity * sizeof(Emitter), nullptr, GL_DYNAMIC_DRAW);
			emitter_buffer_capacity_ = new_capacity;
		}
		if (!emitters.empty()) {
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, emitters.size() * sizeof(Emitter), emitters.data());
		}

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, indirection_buffer_);
		glBufferSubData(
			GL_SHADER_STORAGE_BUFFER,
			0,
			particle_to_emitter_map_.size() * sizeof(int),
			particle_to_emitter_map_.data()
		);

		if (!chunk_info.empty()) {
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, terrain_chunk_buffer_);
			glBufferData(
				GL_SHADER_STORAGE_BUFFER,
				chunk_info.size() * sizeof(glm::vec4),
				chunk_info.data(),
				GL_DYNAMIC_DRAW
			);
		}

		// Reset draw command counts
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

		// Voxel management (for now, just use the first manager for all particles)
		// To truly support multiple buffers, we'd need multiple behavior dispatches or
		// channels in the accumulation SSBO.
		for (auto& vm : voxel_managers_) {
			vm->ClearAccumulation();
			vm->BindResources();
		}

		auto bind_textures_and_uniforms = [&](ComputeShader* shader) {
			shader->use();
			shader->setFloat("u_delta_time", delta_time);
			shader->setFloat("u_time", time_);
			shader->setFloat("u_ambient_density", ambient_density);
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

		if (lighting_ubo != 0) {
			glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::Lighting(), lighting_ubo);
		}

		if (frustum_ubo != 0) {
			glBindBufferRange(
				GL_UNIFORM_BUFFER,
				Constants::UboBinding::FrustumData(),
				frustum_ubo,
				frustum_offset,
				sizeof(FrustumDataGPU)
			);
		}

		// --- Phase 1: Lifecycle ---
		// Handle aging and respawning first so Phase 2/3 work with valid particles
		bind_textures_and_uniforms(lifecycle_shader_.get());
		glDispatchCompute((kMaxParticles / Constants::Class::Particles::ComputeGroupSize()) + 1, 1, 1);

		// Barrier to ensure particles and live indices are updated
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		// --- Phase 2: Build Spatial Grid ---
		// Build grid using the results of Phase 1
		if (grid_build_shader_ && grid_build_shader_->isValid()) {
			grid_build_shader_->use();
			grid_build_shader_->setUint("u_grid_size", Constants::Class::Particles::ParticleGridSize());
			grid_build_shader_->setFloat("u_cell_size", Constants::Class::Particles::ParticleGridCellSize());
			grid_build_shader_->setInt("u_num_particles", kMaxParticles);

			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 16, particle_buffer_);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleGridHeads(), grid_heads_buffer_);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleGridNext(), grid_next_buffer_);

			// Mode 0: Clear
			grid_build_shader_->setInt("u_mode", 0);
			glDispatchCompute(
				(Constants::Class::Particles::ParticleGridSize() / Constants::Class::Particles::ComputeGroupSize()) + 1,
				1,
				1
			);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

			// Mode 1: Build
			grid_build_shader_->setInt("u_mode", 1);
			glDispatchCompute((kMaxParticles / Constants::Class::Particles::ComputeGroupSize()) + 1, 1, 1);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		}

		// --- Phase 3: Command Fixup ---
		fixup_shader_->use();
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 34, behavior_command_buffer_);
		glDispatchCompute(1, 1, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

		// --- Phase 4: Behavior ---
		// Processes only live particles identified in Phase 1, using grid from Phase 2
		bind_textures_and_uniforms(behavior_shader_.get());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleGridHeads(), grid_heads_buffer_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleGridNext(), grid_next_buffer_);

		// Indirect dispatch!
		glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, behavior_command_buffer_);
		glDispatchComputeIndirect(0);
		glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, 0);

		// Ensure memory operations are finished before rendering
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		// Update Voxel system (copy accumulation to sampling)
		for (auto& vm : voxel_managers_) {
			vm->Update(delta_time, time_);
		}

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 16, 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleGridHeads(), 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::ParticleGridNext(), 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 22, 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 17, 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 23, 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 24, 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 27, 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 28, 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 33, 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 34, 0);
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
		int fire_budget = kMaxParticles * 8 / 10; // Reserve 20% for ambient by default
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

		int reserved_start = kMaxParticles * 8 / 10;

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
		GLuint lighting_idx = glGetUniformBlockIndex(render_shader_->ID, "Lighting");
		if (lighting_idx != GL_INVALID_INDEX) {
			glUniformBlockBinding(render_shader_->ID, lighting_idx, Constants::UboBinding::Lighting());
		}

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 22, emitter_buffer_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 27, visible_indices_buffer_);

		if (noise_texture != 0) {
			glActiveTexture(GL_TEXTURE5);
			glBindTexture(GL_TEXTURE_3D, noise_texture);
			render_shader_->setInt("u_noiseTexture", 5);
		}

		if (extra_noise_texture != 0) {
			glActiveTexture(GL_TEXTURE8);
			glBindTexture(GL_TEXTURE_3D, extra_noise_texture);
			render_shader_->setInt("u_extraNoiseTexture", 8);
		}

		// Enable GPU frustum culling for particles
		render_shader_->setBool("enableFrustumCulling", true);
		render_shader_->setFloat("frustumCullRadius", 2.0f); // Particle cull radius

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 16, particle_buffer_);

		// We don't have a VAO for the particles since we generate them in the shader.
		// We use indirect rendering to draw only visible particles.
		// A dummy VAO is required by OpenGL 4.3 core profile.
		glBindVertexArray(dummy_vao_);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, draw_command_buffer_);
		glDrawArraysIndirect(GL_POINTS, 0);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
		glBindVertexArray(0);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 16, 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 22, 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 27, 0);

		glDepthMask(GL_TRUE);                              // Re-enable depth writing
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Reset blend mode
		glDisable(GL_BLEND);
	}

} // namespace Boidsish
