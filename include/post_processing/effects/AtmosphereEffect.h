#pragma once

#include <memory>

#include "post_processing/IPostProcessingEffect.h"

class Shader; // Forward declaration

namespace Boidsish {
	namespace PostProcessing {

		class AtmosphereEffect: public IPostProcessingEffect {
		public:
			AtmosphereEffect();
			~AtmosphereEffect();

			void Apply(
				GLuint           sourceTexture,
				GLuint           depthTexture,
				GLuint           velocityTexture,
				const glm::mat4& viewMatrix,
				const glm::mat4& projectionMatrix,
				const glm::vec3& cameraPos
			) override;
			void Initialize(int width, int height) override;
			void Resize(int width, int height) override;

			void SetTime(float time) override { time_ = time; }

			bool IsEarly() const override { return true; }

			// Haze parameters
			void SetHazeDensity(float density) { haze_density_ = density; }

			float GetHazeDensity() const { return haze_density_; }

			void SetHazeHeight(float height) { haze_height_ = height; }

			float GetHazeHeight() const { return haze_height_; }

			void SetHazeColor(const glm::vec3& color) { haze_color_ = color; }

			glm::vec3 GetHazeColor() const { return haze_color_; }

			// Cloud parameters
			void SetCloudDensity(float density) { cloud_density_ = density; }

			float GetCloudDensity() const { return cloud_density_; }

			void SetCloudAltitude(float altitude) { cloud_altitude_ = altitude; }

			float GetCloudAltitude() const { return cloud_altitude_; }

			void SetCloudThickness(float thickness) { cloud_thickness_ = thickness; }

			float GetCloudThickness() const { return cloud_thickness_; }

			void SetCloudColor(const glm::vec3& color) { cloud_color_ = color; }

			glm::vec3 GetCloudColor() const { return cloud_color_; }

			// Scattering parameters
			void  SetRayleighScale(float s) { rayleigh_scale_ = s; }
			float GetRayleighScale() const { return rayleigh_scale_; }
			void  SetMieScale(float s) { mie_scale_ = s; }
			float GetMieScale() const { return mie_scale_; }
			void  SetMieAnisotropy(float g) { mie_anisotropy_ = g; }
			float GetMieAnisotropy() const { return mie_anisotropy_; }
			void  SetMultiScatScale(float s) { multi_scat_scale_ = s; }
			float GetMultiScatScale() const { return multi_scat_scale_; }
			void  SetAmbientScatScale(float s) { ambient_scat_scale_ = s; }
			float GetAmbientScatScale() const { return ambient_scat_scale_; }

			// Atmosphere LUTs
			void SetAtmosphereLUTs(GLuint transmittance, GLuint multiScat, GLuint skyView, GLuint aerialPerspective) {
				transmittance_lut_ = transmittance;
				multi_scattering_lut_ = multiScat;
				sky_view_lut_ = skyView;
				aerial_perspective_lut_ = aerialPerspective;
			}

		private:
			std::unique_ptr<Shader> shader_;
			float                   time_ = 0.0f;

			float     haze_density_ = 0.003f;
			float     haze_height_ = 20.0f;
			glm::vec3 haze_color_ = glm::vec3(0.6f, 0.7f, 0.8f);
			float     cloud_density_ = 0.5f;
			float     cloud_altitude_ = 175.0f;
			float     cloud_thickness_ = 10.0f;
			glm::vec3 cloud_color_ = glm::vec3(0.95f, 0.95f, 1.0f);

			float rayleigh_scale_ = 1.0f;
			float mie_scale_ = 0.2f; // Reduced default to prevent "washed out" haze
			float mie_anisotropy_ = 0.8f;
			float multi_scat_scale_ = 0.5f; // Reduced default
			float ambient_scat_scale_ = 0.1f;

			GLuint transmittance_lut_ = 0;
			GLuint multi_scattering_lut_ = 0;
			GLuint sky_view_lut_ = 0;
			GLuint aerial_perspective_lut_ = 0;

			int width_ = 0;
			int height_ = 0;
		};

	} // namespace PostProcessing
} // namespace Boidsish
