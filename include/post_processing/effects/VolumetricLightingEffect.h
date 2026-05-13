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

			bool IsEarly() const override { return true; }

			// UI-controlled parameters
			void SetScatteringAnisotropy(float g) { anisotropy_ = g; }
			float GetScatteringAnisotropy() const { return anisotropy_; }

			void SetIntensity(float i) { intensity_ = i; }
			float GetIntensity() const { return intensity_; }

			void SetTemporalAlpha(float a) { temporal_alpha_ = a; }
			float GetTemporalAlpha() const { return temporal_alpha_; }

		private:
			void CreateGridTextures();

			std::unique_ptr<ComputeShader> injection_shader_;
			std::unique_ptr<ComputeShader> integration_shader_;
			std::unique_ptr<Shader> composite_shader_;

			int width_ = 0;
			int height_ = 0;

			// 4 Cascades, each with a Froxel Grid
			// We use a 3D texture array for the 4 cascades
			GLuint injection_texture_ = 0;  // Scattering + Extinction (RGBA16F)
			GLuint scattering_texture_ = 0; // Integrated Scattering + Transmittance (RGBA16F)

			// Temporal accumulation history (at froxel resolution)
			GLuint history_textures_[2] = {0, 0};
			int history_index_ = 0;
			bool has_history_ = false;

			glm::mat4 prev_view_projection_ = glm::mat4(1.0f);

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
