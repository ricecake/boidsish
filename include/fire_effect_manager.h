#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include "IManager.h"
#include "constants.h"
#include "fire_effect.h"
#include "geometry.h"
#include "persistent_buffer.h"
#include "shader.h"

namespace Boidsish {

	class ServiceLocator;

	struct ParticleStats {
		uint32_t count_birds;
		uint32_t count_leaves;
		uint32_t count_petals;
		uint32_t count_bubbles;
		uint32_t count_fireflies;
		uint32_t count_snow;
		uint32_t _unused_counts[2];

		uint32_t limit_birds;
		uint32_t limit_leaves;
		uint32_t limit_petals;
		uint32_t limit_bubbles;
		uint32_t limit_fireflies;
		uint32_t limit_snow;
		uint32_t _unused_limits[2];
	};

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

		void Update(
			float                         delta_time,
			float                         time,
			bool                          enabled = true,
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
		);

		void PrepareUpdate(
			float                         delta_time,
			float                         time,
			bool                          enabled,
			float                         ambient_density,
			const std::vector<glm::vec4>& chunk_info
		);

		void ApplyUpdate(
			float      delta_time,
			bool       enabled,
			float      ambient_density,
			int        num_chunks,
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

		void AdvanceFrame();
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

		// Get current particle stats from the GPU
		ParticleStats GetStats() const;

		// Bind particle buffers to the current shader
		void BindBuffers(ShaderBase& shader) const;

	private:
		void _EnsureShaderAndBuffers();
		void _UpdateParticleAllocation();

		std::vector<std::shared_ptr<FireEffect>> effects_;
		std::vector<int>                         particle_to_emitter_map_;
		mutable std::recursive_mutex             mutex_;

		std::unique_ptr<ComputeShader> lifecycle_shader_;
		std::unique_ptr<ComputeShader> behavior_shader_;
		std::unique_ptr<ComputeShader> fixup_shader_;
		std::unique_ptr<ComputeShader> grid_build_shader_;
		std::unique_ptr<Shader>        render_shader_;

		GLuint particle_buffer_{0};
		GLuint grid_heads_buffer_{0};
		GLuint grid_next_buffer_{0};
		std::unique_ptr<PersistentBuffer<Emitter>>                       emitter_buffer_;
		std::unique_ptr<PersistentBuffer<int>>                           indirection_buffer_;
		std::unique_ptr<PersistentBuffer<glm::vec4>>                     terrain_chunk_buffer_;
		std::unique_ptr<PersistentBuffer<glm::vec4>>                     slice_data_buffer_;
		GLuint                                                           visible_indices_buffer_{0};
		GLuint                                                           live_indices_buffer_{0};
		std::unique_ptr<PersistentBuffer<DrawArraysIndirectCommand>>     draw_command_buffer_;
		std::unique_ptr<PersistentBuffer<uint32_t>>                      behavior_command_buffer_;
		std::unique_ptr<PersistentBuffer<ParticleStats>>                 stats_buffer_;
		GLuint                                                           dummy_vao_{0};

		bool   initialized_{false};
		bool   needs_reallocation_{false};
		float  ambient_density_{0.15f};
		float  time_{0.0f};
		int    num_emitters_last_prepared_{0};
		size_t emitter_buffer_capacity_{0}; // Track capacity to avoid per-frame reallocation

		static constexpr int kMaxParticles = Constants::Class::Particles::MaxParticles();
		static constexpr int kMaxEmitters = Constants::Class::Particles::MaxEmitters();
	};

} // namespace Boidsish
