#pragma once

#include <memory>

#include "NoiseManager.h"
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

			void SetCloudCoverage(float coverage) { cloud_coverage_ = coverage; }

			float GetCloudCoverage() const { return cloud_coverage_; }

			void SetCloudWarp(float warp) { cloud_warp_ = warp; }

			float GetCloudWarp() const { return cloud_warp_; }

			// Scattering parameters
			void SetRayleighScale(float s) { rayleigh_scale_ = s; }

			float GetRayleighScale() const { return rayleigh_scale_; }

			void SetMieScale(float s) { mie_scale_ = s; }

			float GetMieScale() const { return mie_scale_; }

			void SetMieAnisotropy(float g) { mie_anisotropy_ = g; }

			float GetMieAnisotropy() const { return mie_anisotropy_; }

			void SetMultiScatScale(float s) { multi_scat_scale_ = s; }

			float GetMultiScatScale() const { return multi_scat_scale_; }

			void SetAmbientScatScale(float s) { ambient_scat_scale_ = s; }

			float GetAmbientScatScale() const { return ambient_scat_scale_; }

			void SetAtmosphereHeight(float h) { atmosphere_height_ = h; }

			float GetAtmosphereHeight() const { return atmosphere_height_; }

			void SetRayleighScattering(const glm::vec3& s) { rayleigh_scattering_ = s; }

			glm::vec3 GetRayleighScattering() const { return rayleigh_scattering_; }

			void SetMieScattering(float s) { mie_scattering_ = s; }

			float GetMieScattering() const { return mie_scattering_; }

			void SetMieExtinction(float e) { mie_extinction_ = e; }

			float GetMieExtinction() const { return mie_extinction_; }

			void SetOzoneAbsorption(const glm::vec3& a) { ozone_absorption_ = a; }

			glm::vec3 GetOzoneAbsorption() const { return ozone_absorption_; }

			void SetRayleighScaleHeight(float h) { rayleigh_scale_height_ = h; }

			float GetRayleighScaleHeight() const { return rayleigh_scale_height_; }

			void SetMieScaleHeight(float h) { mie_scale_height_ = h; }

			float GetMieScaleHeight() const { return mie_scale_height_; }

			void SetColorVarianceScale(float s) { color_variance_scale_ = s; }

			float GetColorVarianceScale() const { return color_variance_scale_; }

			void SetColorVarianceStrength(float s) { color_variance_strength_ = s; }

			float GetColorVarianceStrength() const { return color_variance_strength_; }

			// Atmosphere LUTs
			void SetAtmosphereLUTs(GLuint transmittance, GLuint multiScat, GLuint skyView, GLuint aerialPerspective) {
				transmittance_lut_ = transmittance;
				multi_scattering_lut_ = multiScat;
				sky_view_lut_ = skyView;
				aerial_perspective_lut_ = aerialPerspective;
			}

			void SetNoiseTextures(const NoiseTextures& textures) { noise_textures_ = textures; }

			void  SetRenderScale(float scale) {
				if (render_scale_ != scale) {
					render_scale_ = scale;
					InitializeLowResResources();
				}
			}

			float GetRenderScale() const { return render_scale_; }

		private:
			void InitializeLowResResources();

			std::unique_ptr<Shader> shader_;
			std::unique_ptr<Shader> composite_shader_;
			float                   time_ = 0.0f;

			float     haze_density_ = 0.003f;
			float     haze_height_ = 20.0f;
			glm::vec3 haze_color_ = glm::vec3(0.6f, 0.7f, 0.8f);
			float     cloud_density_ = 20.0f;
			float     cloud_altitude_ = 400.0f;
			float     cloud_thickness_ = 200.0f;
			float     cloud_coverage_ = 0.75f;
			float     cloud_warp_ = 75.0f;
			glm::vec3 cloud_color_ = glm::vec3(0.95f, 0.95f, 1.0f);

			float     rayleigh_scale_ = 1.0f;
			float     mie_scale_ = 0.1f; // Reduced default to prevent "washed out" haze
			float     mie_anisotropy_ = 0.8f;
			float     multi_scat_scale_ = 0.1f;
			float     ambient_scat_scale_ = 0.750f;
			float     atmosphere_height_ = 120.0f;
			glm::vec3 rayleigh_scattering_ = glm::vec3(5.802f, 13.558f, 33.100f) * 1e-3f;
			float     mie_scattering_ = 3.996f * 1e-3f;
			float     mie_extinction_ = 4.440f * 1e-3f;
			glm::vec3 ozone_absorption_ = glm::vec3(0.650f, 1.881f, 0.085f) * 1e-3f;
			float     rayleigh_scale_height_ = 8.0f;
			float     mie_scale_height_ = 1.2f;
			float     color_variance_scale_ = 1.0f;
			float     color_variance_strength_ = 0.0f;

			GLuint transmittance_lut_ = 0;
			GLuint multi_scattering_lut_ = 0;
			GLuint sky_view_lut_ = 0;
			GLuint aerial_perspective_lut_ = 0;

			NoiseTextures noise_textures_ = {0, 0, 0};

			int   width_ = 0;
			int   height_ = 0;
			float render_scale_ = 0.25f;

			GLuint low_res_fbo_ = 0;
			GLuint low_res_texture_ = 0;
		};

	} // namespace PostProcessing
} // namespace Boidsish
