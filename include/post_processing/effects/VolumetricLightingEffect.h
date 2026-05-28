#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include "post_processing/IPostProcessingEffect.h"
#include "shader.h"

namespace Boidsish {
	namespace PostProcessing {

		class VolumetricLightingEffect : public IPostProcessingEffect {
		public:
			VolumetricLightingEffect();
			~VolumetricLightingEffect();

			void Initialize(int width, int height) override;
			void Resize(int width, int height) override;
			void Apply(GLuint sourceTexture, GLuint depthTexture, GLuint velocityTexture, GLuint normalTexture, GLuint albedoTexture, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraPos) override;
			void PreDispatch(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraPos) override;

			bool IsEarly() const override { return true; }

			// UI-controlled parameters
			void SetScatteringAnisotropy(float g) { anisotropy_ = g; }
			float GetScatteringAnisotropy() const { return anisotropy_; }

			void SetIntensity(float i) { intensity_ = i; }
			float GetIntensity() const { return intensity_; }

			void SetTemporalAlpha(float a) { temporal_alpha_ = a; }
			float GetTemporalAlpha() const { return temporal_alpha_; }

			GLuint GetInjectionBuffer() const { return injection_buffer_; }
			GLuint GetInjectionTexture() const { return injection_texture_; }
			glm::ivec3 GetGridResolution() const { return glm::ivec3(grid_res_x_, grid_res_y_, grid_res_z_); }

		private:
			void CreateGridTextures();

			std::unique_ptr<ComputeShader> injection_shader_;
			std::unique_ptr<ComputeShader> splat_shader_;
			std::unique_ptr<ComputeShader> integration_shader_;
			std::unique_ptr<Shader> composite_shader_;

			int width_ = 0;
			int height_ = 0;

			// 4 Cascades, each with a Froxel Grid
			// We use a 3D texture array for the 4 cascades
			GLuint injection_buffer_ = 0;   // VolumetricData SSBO
			GLuint injection_texture_ = 0;  // Pre-integration light sources (RGBA16F)
			GLuint scattering_texture_ = 0; // Integrated Scattering + Transmittance (RGBA16F)

			// Temporal accumulation history (at froxel resolution)
			GLuint history_textures_[2] = {0, 0};
			int history_index_ = 0;
			bool has_history_ = false;

			glm::mat4 prev_view_projection_ = glm::mat4(1.0f);
			glm::vec3 prev_camera_pos_ = glm::vec3(0.0f);
			glm::vec3 prev_camera_front_ = glm::vec3(0.0f, 0.0f, -1.0f);

			float anisotropy_ = 0.8f;
			float intensity_ = 1.0f;
			float temporal_alpha_ = 0.95f;

			// Froxel grid dimensions per cascade
			const int grid_res_x_ = 160;
			const int grid_res_y_ = 90;
			const int grid_res_z_ = 64;
			const int num_cascades_ = 4;
		};

	} // namespace PostProcessing
} // namespace Boidsish
