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
			void PreDispatch(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraPos) override;
			void Apply(GLuint sourceTexture, GLuint depthTexture, GLuint velocityTexture, GLuint normalTexture, GLuint albedoTexture, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraPos) override;

			bool IsEarly() const override { return true; }

			// UI-controlled parameters
			void SetScatteringAnisotropyG1(const glm::vec3& g) { anisotropy_g1_ = g; }
			glm::vec3 GetScatteringAnisotropyG1() const { return anisotropy_g1_; }

			void SetScatteringAnisotropyG2(const glm::vec3& g) { anisotropy_g2_ = g; }
			glm::vec3 GetScatteringAnisotropyG2() const { return anisotropy_g2_; }

			void SetScatteringAnisotropyAlpha(float a) { anisotropy_alpha_ = a; }
			float GetScatteringAnisotropyAlpha() const { return anisotropy_alpha_; }

			void SetScatteringAnisotropyIsotropic(float i) { anisotropy_isotropic_ = i; }
			float GetScatteringAnisotropyIsotropic() const { return anisotropy_isotropic_; }

			void SetIntensity(float i) { intensity_ = i; }
			float GetIntensity() const { return intensity_; }

			void SetTemporalAlpha(float a) { temporal_alpha_ = a; }
			float GetTemporalAlpha() const { return temporal_alpha_; }

			void SetAmbientScale(float s) { ambient_scale_ = s; }
			float GetAmbientScale() const { return ambient_scale_; }

			void SetRayleighScale(float s) { rayleigh_scale_ = s; }
			float GetRayleighScale() const { return rayleigh_scale_; }

			void SetMieScale(float s) { mie_scale_ = s; }
			float GetMieScale() const { return mie_scale_; }

			void SetMultiScatScale(float s) { multi_scat_scale_ = s; }
			float GetMultiScatScale() const { return multi_scat_scale_; }

			void SetShadowSensitivity(float s) { shadow_sensitivity_ = s; }
			float GetShadowSensitivity() const { return shadow_sensitivity_; }

			void SetTemporalAccumulation2D(bool b) { temporal_accumulation_2d_ = b; }
			bool GetTemporalAccumulation2D() const { return temporal_accumulation_2d_; }

		private:
			void CreateGridTextures();
			void Create2DHistoryTextures(int w, int h);

			std::unique_ptr<ComputeShader> injection_shader_;
			std::unique_ptr<ComputeShader> integration_shader_;
			std::unique_ptr<Shader> composite_shader_;
			std::unique_ptr<ComputeShader> accumulation_2d_shader_;

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

			// 2D Temporal accumulation history (for screenspace temporal accumulation)
			GLuint history_textures_2d_[2] = {0, 0};
			int history_index_2d_ = 0;
			bool has_history_2d_ = false;

			glm::mat4 prev_view_projection_ = glm::mat4(1.0f);
			glm::vec3 prev_camera_pos_ = glm::vec3(0.0f);
			glm::vec3 prev_camera_front_ = glm::vec3(0.0f, 0.0f, -1.0f);

			glm::vec3 anisotropy_g1_ = glm::vec3(0.8f);
			glm::vec3 anisotropy_g2_ = glm::vec3(-0.3f);
			float anisotropy_alpha_ = 0.181f;
			float anisotropy_isotropic_ = 0.426f;
			float intensity_ = 1.0f;
			float temporal_alpha_ = 0.95f;

			float ambient_scale_ = 1.0f;
			float rayleigh_scale_ = 1.0f;
			float mie_scale_ = 1.0f;
			float multi_scat_scale_ = 1.0f;
			float shadow_sensitivity_ = 1.0f;
			bool temporal_accumulation_2d_ = true;

			// Froxel grid dimensions per cascade
			const int grid_res_x_ = 160;
			const int grid_res_y_ = 90;
			const int grid_res_z_ = 64;
			const int num_cascades_ = 4;
		};

	} // namespace PostProcessing
} // namespace Boidsish
