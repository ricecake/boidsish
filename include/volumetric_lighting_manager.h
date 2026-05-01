#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include "IManager.h"

class ComputeShader;

namespace Boidsish {

	class ServiceLocator;

	struct alignas(16) VolumetricLightingParams {
		glm::mat4 view_projection;
		glm::mat4 inv_view_projection;
		glm::vec4 camera_pos_near; // xyz, w: near
		glm::vec4 camera_dir_far;  // xyz, w: far
		glm::ivec4 grid_res_pad;   // xyz, w: pad
	};

	class VolumetricLightingManager : public IManager {
	public:
		VolumetricLightingManager(ServiceLocator& loc);
		~VolumetricLightingManager();

		void Initialize() override;

		void Update(
			const glm::mat4& view_proj,
			const glm::mat4& inv_view_proj,
			const glm::vec3& cam_pos,
			const glm::vec3& cam_dir,
			float near_p,
			float far_p,
			float simulation_time
		);

		void Clear();

		void InjectParticles(
			GLuint particle_buffer,
			int max_particles,
			GLuint live_indices_buffer,
			GLuint behavior_command_buffer
		);

		void Resolve();

		GLuint GetVolumetricTexture() const { return volumetric_texture_; }
		const glm::ivec3& GetGridResolution() const { return grid_res_; }

		void BindForSampling(GLuint unit);

	private:
		void CreateResources();
		void CreateShaders();

		GLuint volumetric_texture_ = 0;
		GLuint accumulation_ssbo_ = 0; // uint3 SSBO for atomic radiance accumulation
		GLuint params_ubo_ = 0;

		std::unique_ptr<ComputeShader> clear_shader_;
		std::unique_ptr<ComputeShader> inject_shader_;
		std::unique_ptr<ComputeShader> resolve_shader_;

		glm::ivec3 grid_res_ = {128, 72, 64};
		VolumetricLightingParams params_;
		float simulation_time_ = 0.0f;
	};

} // namespace Boidsish
