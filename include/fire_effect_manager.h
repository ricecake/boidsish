#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include "constants.h"
#include "fire_effect.h"
#include "shader.h"
#include "voxel_brick_manager.h"

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

	class FireEffectManager {
	public:
		FireEffectManager(int num_voxel_buffers = 1);
		~FireEffectManager();

		// Initialize shaders and buffers. Must be called from main thread with OpenGL context.
		void Initialize();

		// Returns true if fire effects are available (compute shader compiled successfully)
		bool IsAvailable() const;

		void Update(
			float                         delta_time,
			float                         time,
			float                         ambient_density = 0.15f,
			const std::vector<glm::vec4>& chunk_info = {},
			GLuint                        heightmap_texture = 0,
			GLuint                        curl_noise_texture = 0,
			GLuint                        biome_texture = 0,
			GLuint                        lighting_ubo = 0,
			GLuint                        frustum_ubo = 0,
			GLintptr                      frustum_offset = 0,
			GLuint                        extra_noise_texture = 0
		);
		void Render(
			const glm::mat4& view,
			const glm::mat4& projection,
			const glm::vec3& camera_pos,
			GLuint           noise_texture = 0,
			GLuint           extra_noise_texture = 0
		);

		// Add a new fire effect and return a pointer to it
		VoxelBrickManager* GetVoxelManager(int index = 0) {
			return index < voxel_managers_.size() ? voxel_managers_[index].get() : nullptr;
		}

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
		std::vector<std::unique_ptr<VoxelBrickManager>> voxel_managers_;

		GLuint particle_buffer_{0};
		GLuint grid_heads_buffer_{0};
		GLuint grid_next_buffer_{0};
		GLuint emitter_buffer_{0};
		GLuint indirection_buffer_{0};
		GLuint terrain_chunk_buffer_{0};
		GLuint slice_data_buffer_{0};
		GLuint visible_indices_buffer_{0};
		GLuint live_indices_buffer_{0};
		GLuint draw_command_buffer_{0};
		GLuint behavior_command_buffer_{0};
		GLuint dummy_vao_{0};

		bool   initialized_{false};
		bool   needs_reallocation_{false};
		float  time_{0.0f};
		size_t emitter_buffer_capacity_{0}; // Track capacity to avoid per-frame reallocation

		static const int kMaxParticles = Constants::Class::Particles::MaxParticles();
		static const int kMaxEmitters = Constants::Class::Particles::MaxEmitters();
	};

} // namespace Boidsish
