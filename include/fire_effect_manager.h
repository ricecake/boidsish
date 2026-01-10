#pragma once

#include <memory>
#include <queue>
#include <vector>

#include "fire_effect.h"
#include "shader.h"

namespace Boidsish {
	class Visualizer;

	// This struct is mirrored in the compute shader.
	// It must match the layout and padding there.
	struct alignas(16) Emitter {
		glm::vec4 position;
		glm::vec3 direction;
		int       style;
		glm::vec3 velocity;
		int       is_active;
		glm::vec3 terrain_texture_origin;
		int       terrain_texture_layer;
	};

	constexpr int kTerrainTextureRange = 100;

	class FireEffectManager {
	public:
		FireEffectManager();
		~FireEffectManager();

		void Update(float delta_time, float time, Visualizer& visualizer);
		void Render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camera_pos);

		// Add a new fire effect and return a pointer to it
		std::shared_ptr<FireEffect> AddEffect(
			const glm::vec3& position,
			FireEffectStyle  style,
			const glm::vec3& direction = glm::vec3(0.0f),
			const glm::vec3& velocity = glm::vec3(0.0f),
			int              max_particles = -1,
			float            lifetime = -1.0f,
			bool             needs_terrain_data = false
		);
		void RemoveEffect(const std::shared_ptr<FireEffect>& effect);
		void PreloadTerrainData(const glm::vec3& center, Visualizer& visualizer);

	private:
		void _EnsureShaderAndBuffers();
		void _UpdateParticleAllocation();

		std::vector<std::shared_ptr<FireEffect>> effects_;
		std::vector<int>                         particle_to_emitter_map_;
		std::queue<int>                          available_texture_layers_;

		std::unique_ptr<ComputeShader> compute_shader_;
		std::unique_ptr<Shader>        render_shader_;

		GLuint particle_buffer_{0};
		GLuint emitter_buffer_{0};
		GLuint indirection_buffer_{0};
		GLuint terrain_texture_{0};
		GLuint dummy_vao_{0};

		bool      initialized_{false};
		float     time_{0.0f};

		static const int kMaxParticles = 32000;
		static const int kMaxEmitters = 100;
	};

} // namespace Boidsish