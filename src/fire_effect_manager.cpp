#include "fire_effect_manager.h"

#include <algorithm>
#include <numeric>
#include <queue>

#include "graphics.h" // For logger
#include "logger.h"
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>

namespace Boidsish {

	// This must match the struct in the compute shader
	struct Particle {
		glm::vec4 pos;
		glm::vec4 vel;
		int       style;
		int       emitter_index;
		glm::vec2 _padding;
	};

	FireEffectManager::FireEffectManager() {}

	FireEffectManager::~FireEffectManager() {
		if (particle_buffer_ != 0) {
			glDeleteBuffers(1, &particle_buffer_);
		}
		if (emitter_buffer_ != 0) {
			glDeleteBuffers(1, &emitter_buffer_);
		}
		if (indirection_buffer_ != 0) {
			glDeleteBuffers(1, &indirection_buffer_);
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
		compute_shader_ = std::make_unique<ComputeShader>("shaders/fire.comp");
		render_shader_ = std::make_unique<Shader>("shaders/fire.vert", "shaders/fire.frag");

		// Create buffers
		glGenBuffers(1, &particle_buffer_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, particle_buffer_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxParticles * sizeof(Particle), nullptr, GL_DYNAMIC_DRAW);

		glGenBuffers(1, &emitter_buffer_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, emitter_buffer_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxEmitters * sizeof(Emitter), nullptr, GL_DYNAMIC_DRAW);

		glGenBuffers(1, &indirection_buffer_);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, indirection_buffer_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxParticles * sizeof(int), nullptr, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		particle_to_emitter_map_.resize(kMaxParticles, -1);

		// A dummy VAO is required by OpenGL 4.2 core profile for drawing arrays.
		glGenVertexArrays(1, &dummy_vao_);

		initialized_ = true;
	}

	std::shared_ptr<FireEffect> FireEffectManager::AddEffect(
		const glm::vec3& position,
		FireEffectStyle  style,
		const glm::vec3& direction,
		const glm::vec3& velocity,
		int              max_particles,
		float            lifetime
	) {
		_EnsureShaderAndBuffers();

		// Find an inactive slot to reuse
		for (size_t i = 0; i < effects_.size(); ++i) {
			if (!effects_[i]) {
				effects_[i] =
					std::make_shared<FireEffect>(position, style, direction, velocity, max_particles, lifetime);
				_UpdateParticleAllocation();
				return effects_[i];
			}
		}

		// If no inactive slots, add a new one if under capacity
		if (effects_.size() < kMaxEmitters) {
			auto effect = std::make_shared<FireEffect>(position, style, direction, velocity, max_particles, lifetime);
			effects_.push_back(effect);
			_UpdateParticleAllocation();
			return effect;
		}

		logger::ERROR("Maximum number of fire effects reached.");
		return nullptr;
	}

	void FireEffectManager::RemoveEffect(const std::shared_ptr<FireEffect>& effect) {
		if (effect) {
			for (size_t i = 0; i < effects_.size(); ++i) {
				if (effects_[i] == effect) {
					effects_[i] = nullptr; // Mark as inactive
					_UpdateParticleAllocation();
					return;
				}
			}
		}
	}

	void FireEffectManager::Update(float delta_time, float time) {
		if (!initialized_) {
			return;
		}

		time_ = time;
		// --- Effect Lifetime Management ---
		bool needs_reallocation = false;
		for (auto& effect : effects_) {
			if (effect) {
				float lifetime = effect->GetLifetime();
				if (lifetime > 0.0f) {
					float lived = effect->GetLived();
					lived += delta_time;
					effect->SetLived(lived);
					if (lived >= lifetime) {
						effect = nullptr; // Mark for removal
						needs_reallocation = true;
					}
				}
			}
		}

		if (needs_reallocation) {
			_UpdateParticleAllocation();
		}

		// --- Update Emitters ---
		std::vector<Emitter> emitters;
		emitters.reserve(effects_.size());
		for (const auto& effect : effects_) {
			if (effect) {
				emitters.push_back(
					{effect->GetPosition(),
				     (int)effect->GetStyle(),
				     effect->GetDirection(),
				     1, // is_active
				     effect->GetVelocity(),
				     0.0f}
				);
			} else {
				// Add a placeholder for inactive emitters to maintain indexing
				emitters.push_back({glm::vec3(0), 0, glm::vec3(0), 0, glm::vec3(0), 0.0f});
			}
		}

		if (emitters.empty()) {
			return; // No active emitters, nothing to compute
		}

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, emitter_buffer_);
		glBufferData(GL_SHADER_STORAGE_BUFFER, emitters.size() * sizeof(Emitter), emitters.data(), GL_DYNAMIC_DRAW);

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, indirection_buffer_);
		glBufferSubData(
			GL_SHADER_STORAGE_BUFFER,
			0,
			particle_to_emitter_map_.size() * sizeof(int),
			particle_to_emitter_map_.data()
		);

		// --- Dispatch Compute Shader ---
		compute_shader_->use();
		compute_shader_->setFloat("u_delta_time", delta_time);
		compute_shader_->setFloat("u_time", time_);
		compute_shader_->setInt("u_num_emitters", emitters.size());

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particle_buffer_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, emitter_buffer_);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, indirection_buffer_);

		// Dispatch enough groups to cover all particles
		glDispatchCompute((kMaxParticles / 256) + 1, 1, 1);

		// Ensure memory operations are finished before rendering
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, 0);
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

		if (num_active_emitters == 0)
			return;

		int avg_particles_per_unlimited = 0;
		if (num_unlimited_emitters > 0) {
			int available_for_unlimited = kMaxParticles - total_particle_demand;
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
		int remainder = kMaxParticles - current_total;
		for (size_t i = 0; i < effects_.size() && remainder > 0; ++i) {
			if (effects_[i]) {
				ideal_counts[i]++;
				remainder--;
			}
		}

		// --- 2. Calculate Current Distribution ---
		std::vector<int> current_counts(effects_.size(), 0);
		std::vector<int> null_particles;
		for (int i = 0; i < kMaxParticles; ++i) {
			int emitter_index = particle_to_emitter_map_[i];
			if (emitter_index != -1 && emitter_index < (int)effects_.size() && effects_[emitter_index]) {
				current_counts[emitter_index]++;
			} else {
				null_particles.push_back(i);
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
				// Find -diff particles to reclaim
				int count = -diff;
				for (int p_idx = 0; p_idx < kMaxParticles && count > 0; ++p_idx) {
					if (particle_to_emitter_map_[p_idx] == (int)i) {
						to_reclaim.push_back(p_idx);
						count--;
					}
				}
			}
		}

		// --- 4. Perform Stable Re-mapping ---
		// First, use null particles to fill under-budget emitters
		while (!to_fill.empty() && !null_particles.empty()) {
			int emitter_index = to_fill.top().second;
			to_fill.pop();
			int particle_index = null_particles.back();
			null_particles.pop_back();

			particle_to_emitter_map_[particle_index] = emitter_index;
			ideal_counts[emitter_index]--;
			if (ideal_counts[emitter_index] > current_counts[emitter_index]) {
				float need = (float)(ideal_counts[emitter_index] - current_counts[emitter_index]) /
					ideal_counts[emitter_index];
				to_fill.push({need, emitter_index});
			}
		}

		// Second, use reclaimed particles to fill the rest
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

	void FireEffectManager::Render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camera_pos) {
		if (!initialized_ || effects_.empty()) {
			return;
		}

		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // Additive blending for fire
		glDepthMask(GL_FALSE);                       // Disable depth writing

		render_shader_->use();
		render_shader_->setMat4("u_view", view);
		render_shader_->setMat4("u_projection", projection);
		render_shader_->setVec3("u_camera_pos", camera_pos);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particle_buffer_);

		// We don't have a VAO for the particles since we generate them in the shader.
		// We can just draw the number of particles we have.
		// A dummy VAO is required by OpenGL 4.2 core profile.
		glBindVertexArray(dummy_vao_);
		glDrawArrays(GL_POINTS, 0, kMaxParticles);
		glBindVertexArray(0);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);

		glDepthMask(GL_TRUE);                              // Re-enable depth writing
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Reset blend mode
	}

} // namespace Boidsish
