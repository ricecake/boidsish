#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include "IManager.h"
#include "constants.h"
#include "fire_effect.h"
#include "persistent_buffer.h"
#include "shader.h"
#include "geometry.h"

namespace Boidsish {

	class ServiceLocator;

	// This struct is mirrored in the compute shader.
	// It must match the layout and padding there.
	struct ChunkInfo {
		glm::vec2 worldOffset;
		float     slice;
		float     size;
	};

	// This struct is mirrored in the compute shader.
	// It must match the layout and padding there.
	struct Emitter {
		glm::vec3 position;   // 12 bytes
		int       style;      // 4 bytes -> total 16
		glm::vec3 direction;  // 12 bytes
		int       is_active;  // 4 bytes -> total 16
		glm::vec3 velocity;   // 12 bytes
		int       id;         // 4 bytes -> total 16
		glm::vec3 dimensions; // 12 bytes
		int       type;       // 4 bytes -> total 16
		float     sweep;      // 4 bytes
		int       use_slice_data;
		int       slice_data_offset;
		int       slice_data_count;
		float     slice_area;
		int       request_clear;
		int       _padding_emitter[2];
	};

	class FireEffectManager: public IManager {
	public:
		FireEffectManager(ServiceLocator& loc);
		~FireEffectManager();

		// Initialize shaders and buffers. Must be called from main thread with OpenGL context.
		void Initialize() override;

		// Returns true if fire effects are available (compute shader compiled successfully)
		bool IsAvailable() const;

		void AdvanceFrame();

		/**
		 * @brief CPU-side update for particle allocation and data preparation.
		 * Can be called from a background thread or during the logical update phase.
		 */
		void UpdateCPU(
			float                         delta_time,
			float                         time,
			float                         ambient_density,
			const std::vector<glm::vec4>& chunk_info
		);

		/**
		 * @brief GPU-side update involving compute shader dispatches.
		 * Must be called from the main thread with an active OpenGL context.
		 */
		void UpdateGPU(
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
		);

		// Deprecated combined update
		void Update(
			float                         delta_time,
			float                         time,
			float                         ambient_density = 0.15f,
			const std::vector<glm::vec4>& chunk_info = {},
			GLuint                        heightmap_texture = 0,
			GLuint                        curl_noise_texture = 0,
			GLuint                        biome_texture = 0,
			GLuint                        lighting_ubo = 0,
			GLintptr                      lighting_ubo_offset = 0,
			GLsizeiptr                    lighting_ubo_size = 0,
			GLuint                        frustum_ubo = 0,
			GLintptr                      frustum_offset = 0,
			GLuint                        extra_noise_texture = 0,
			GLuint                        visual_effects_ubo = 0,
			GLintptr                      vfx_offset = 0,
			GLsizeiptr                    vfx_size = 0
		) {
			UpdateCPU(delta_time, time, ambient_density, chunk_info);
			UpdateGPU(delta_time, heightmap_texture, curl_noise_texture, biome_texture,
				lighting_ubo, lighting_ubo_offset, lighting_ubo_size,
				frustum_ubo, frustum_offset, extra_noise_texture,
				visual_effects_ubo, vfx_offset, vfx_size);
		}

		void Render(
			const glm::mat4& view,
			const glm::mat4& projection,
			const glm::vec3& camera_pos,
			GLuint           noise_texture = 0,
			GLuint           extra_noise_texture = 0
		);

		// Add a new fire effect and return a pointer to it
		std::shared_ptr<FireEffect> AddEffect(
			const glm::vec3& position,
			FireEffectStyle  style,
			const glm::vec3& direction = glm::vec3(0.0f),
			const glm::vec3& velocity = glm::vec3(0.0f),
			int              max_particles = -1,
			float            lifetime = -1.0f,
			EmitterType      type = EmitterType::Point,
			const glm::vec3& dimensions = glm::vec3(0.0f),
			float            sweep = 1.0f
		);
		void RemoveEffect(const std::shared_ptr<FireEffect>& effect);

	private:
		void _EnsureShaderAndBuffers();
		void _UpdateParticleAllocation();

		std::vector<std::shared_ptr<FireEffect>> effects_;
		std::vector<int>                         particle_to_emitter_map_;
		mutable std::mutex                       mutex_;

		std::unique_ptr<ComputeShader> lifecycle_shader_;
		std::unique_ptr<ComputeShader> behavior_shader_;
		std::unique_ptr<ComputeShader> fixup_shader_;
		std::unique_ptr<ComputeShader> grid_build_shader_;
		std::unique_ptr<Shader>        render_shader_;

		GLuint particle_buffer_{0};
		GLuint grid_heads_buffer_{0};
		GLuint grid_next_buffer_{0};
		std::unique_ptr<PersistentBuffer<Emitter>>                    emitter_pb_;
		std::unique_ptr<PersistentBuffer<int>>                        indirection_pb_;
		std::unique_ptr<PersistentBuffer<glm::vec4>>                  terrain_chunk_pb_;
		std::unique_ptr<PersistentBuffer<glm::vec4>>                  slice_data_pb_;
		GLuint                                                        visible_indices_buffer_{0};
		GLuint                                                        live_indices_buffer_{0};
		std::unique_ptr<PersistentBuffer<DrawArraysIndirectCommand>>  draw_command_pb_;
		std::unique_ptr<PersistentBuffer<uint32_t>>                   behavior_command_pb_; // DispatchIndirectCommand
		GLuint dummy_vao_{0};

		bool   initialized_{false};
		bool   needs_reallocation_{false};
		float  ambient_density_{0.15f};
		float  time_{0.0f};
		int    num_active_chunks_{0};
		size_t emitter_buffer_capacity_{0}; // Track capacity to avoid per-frame reallocation

		static constexpr int kMaxParticles = Constants::Class::Particles::MaxParticles();
		static constexpr int kMaxEmitters = Constants::Class::Particles::MaxEmitters();
	};

} // namespace Boidsish
