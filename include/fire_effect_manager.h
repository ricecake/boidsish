#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include "constants.h"
#include "fire_effect.h"
#include "shader.h"

namespace Boidsish {

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
		glm::vec3 position;
		int       style;
		glm::vec3 direction;
		int       is_active;
		glm::vec3 velocity;
		int       id;
	};

	class FireEffectManager {
	public:
		FireEffectManager();
		~FireEffectManager();

		// Initialize shaders and buffers. Must be called from main thread with OpenGL context.
		void Initialize();

		// Returns true if fire effects are available (compute shader compiled successfully)
		bool IsAvailable() const;

		void Update(
			float                         delta_time,
			float                         time,
			const std::vector<glm::vec4>& chunk_info = {},
			GLuint                        heightmap_texture = 0,
			GLuint                        curl_noise_texture = 0,
			GLuint                        biome_texture = 0,
			GLuint                        lighting_ubo = 0
		);
		void Render(
			const glm::mat4& view,
			const glm::mat4& projection,
			const glm::vec3& camera_pos,
			GLuint           noise_texture = 0
		);

		// Add a new fire effect and return a pointer to it
		std::shared_ptr<FireEffect> AddEffect(
			const glm::vec3& position,
			FireEffectStyle  style,
			const glm::vec3& direction = glm::vec3(0.0f),
			const glm::vec3& velocity = glm::vec3(0.0f),
			int              max_particles = -1,
			float            lifetime = -1.0f
		);
		void RemoveEffect(const std::shared_ptr<FireEffect>& effect);

	private:
		void _EnsureShaderAndBuffers();
		void _UpdateParticleAllocation();

		std::vector<std::shared_ptr<FireEffect>> effects_;
		std::vector<int>                         particle_to_emitter_map_;
		mutable std::mutex                       mutex_;

		std::unique_ptr<ComputeShader> compute_shader_;
		std::unique_ptr<Shader>        render_shader_;

		GLuint particle_buffer_{0};
		GLuint emitter_buffer_{0};
		GLuint indirection_buffer_{0};
		GLuint terrain_chunk_buffer_{0};
		GLuint dummy_vao_{0};

		bool   initialized_{false};
		bool   needs_reallocation_{false};
		float  time_{0.0f};
		size_t emitter_buffer_capacity_{0}; // Track capacity to avoid per-frame reallocation
		size_t terrain_chunk_buffer_capacity_{0};

		static const int kMaxParticles = Constants::Class::Particles::MaxParticles();
		static const int kMaxEmitters = Constants::Class::Particles::MaxEmitters();
	};

} // namespace Boidsish
