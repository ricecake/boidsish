#pragma once

#include <memory>
#include <vector>

#include "NoiseManager.h"
#include "light.h"
#include "post_processing/IPostProcessingEffect.h"

class Shader;
class ComputeShader;

namespace Boidsish {
	namespace PostProcessing {

		enum class VolumetricTechnique {
			Grid,   // Robust low-res 2D grid
			Epipolar // Optimized line-based
		};

		class VolumetricLightingEffect: public IPostProcessingEffect {
		public:
			VolumetricLightingEffect();
			~VolumetricLightingEffect();

			void Apply(GLuint sourceTexture, GLuint depthTexture, GLuint velocityTexture, GLuint normalTexture, GLuint albedoTexture, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraPos) override;
			void Initialize(int width, int height) override;
			void Resize(int width, int height) override;

			void SetTime(float time) override { time_ = time; }

			bool IsEarly() const override { return false; }

			// Parameters
			void  SetScatteringCoefficient(float s) { scattering_coef_ = s; }
			float GetScatteringCoefficient() const { return scattering_coef_; }

			void  SetAbsorptionCoefficient(float a) { absorption_coef_ = a; }
			float GetAbsorptionCoefficient() const { return absorption_coef_; }

			void  SetPhaseG(float g) { phase_g_ = g; }
			float GetPhaseG() const { return phase_g_; }

			void  SetMaxRayDistance(float d) { max_ray_distance_ = d; }
			float GetMaxRayDistance() const { return max_ray_distance_; }

			void SetSampleCount(int count) { sample_count_ = count; }

			void  SetIntensity(float i) { intensity_ = i; }
			float GetIntensity() const { return intensity_; }

			void  SetTechnique(VolumetricTechnique t) { technique_ = t; }
			VolumetricTechnique GetTechnique() const { return technique_; }

			void  SetHazeDensity(float d) { haze_density_ = d; }
			float GetHazeDensity() const { return haze_density_; }

			void  SetHazeHeight(float h) { haze_height_ = h; }
			float GetHazeHeight() const { return haze_height_; }

			void SetHazeParams(float density, float height) {
				haze_density_ = density;
				haze_height_ = height;
			}

			void SetNoiseTextures(const NoiseTextures& textures) { noise_textures_ = textures; }
			void SetLights(const std::vector<Light>& lights) { lights_ = lights; }
			void SetShadowMapTexture(GLuint tex) { shadow_map_array_ = tex; }

		private:
			void InitializeResources();

			std::unique_ptr<ComputeShader> grid_shader_;
			std::unique_ptr<ComputeShader> epipolar_shader_;
			std::unique_ptr<ComputeShader> reproject_shader_;
			std::unique_ptr<ComputeShader> blur_shader_;
			std::unique_ptr<Shader>        composite_shader_;

			int   width_ = 0;
			int   height_ = 0;
			float time_ = 0.0f;

			float scattering_coef_ = 0.1f;
			float absorption_coef_ = 0.01f;
			float phase_g_ = 0.8f;
			float max_ray_distance_ = 1000.0f;
			int   sample_count_ = 64;
			float intensity_ = 1.0f;

			float haze_density_ = 1.0f;
			float haze_height_ = 20.0f;

			VolumetricTechnique technique_ = VolumetricTechnique::Grid;

			// Resources
			int    num_lines_ = 256;
			int    samples_per_line_ = 512;
			GLuint epipolar_tex_ = 0;
			GLuint volumetric_tex_ = 0; // Low-res buffer
			GLuint blurred_tex_ = 0;    // Filtered buffer

			struct LightMetadata {
				int   index;
				float weight;
			};
			std::vector<LightMetadata> GetSignificantLights(const glm::vec3& cameraPos);

			NoiseTextures      noise_textures_ = {0, 0, 0};
			std::vector<Light> lights_;
			GLuint             shadow_map_array_ = 0;
		};

	} // namespace PostProcessing
} // namespace Boidsish
