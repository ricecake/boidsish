#pragma once

#include <memory>

#include "post_processing/IPostProcessingEffect.h"

class Shader; // Forward declaration
class ComputeShader;

namespace Boidsish {
	namespace PostProcessing {

		class AtmosphereEffect: public IPostProcessingEffect {
		public:
			AtmosphereEffect();
			~AtmosphereEffect();

			void Apply(
				GLuint           sourceTexture,
				GLuint           depthTexture,
				const glm::mat4& viewMatrix,
				const glm::mat4& projectionMatrix,
				const glm::vec3& cameraPos
			) override;
			void Initialize(int width, int height) override;
			void Resize(int width, int height) override;

			void SetTime(float time) override { time_ = time; }

			// Haze parameters
			void SetHazeDensity(float density) { haze_density_ = density; }

			float GetHazeDensity() const { return haze_density_; }

			void SetHazeHeight(float height) { haze_height_ = height; }

			float GetHazeHeight() const { return haze_height_; }

			void SetHazeColor(const glm::vec3& color) { haze_color_ = color; }

			glm::vec3 GetHazeColor() const { return haze_color_; }

			void SetHazeG(float g) { haze_g_ = g; }

			float GetHazeG() const { return haze_g_; }

			// Cloud parameters
			void SetCloudDensity(float density) { cloud_density_ = density; }

			float GetCloudDensity() const { return cloud_density_; }

			void SetCloudAltitude(float altitude) { cloud_altitude_ = altitude; }

			float GetCloudAltitude() const { return cloud_altitude_; }

			void SetCloudThickness(float thickness) { cloud_thickness_ = thickness; }

			float GetCloudThickness() const { return cloud_thickness_; }

			void SetCloudColor(const glm::vec3& color) { cloud_color_ = color; }

			glm::vec3 GetCloudColor() const { return cloud_color_; }

			void SetCloudG(float g) { cloud_g_ = g; }

			float GetCloudG() const { return cloud_g_; }

			void SetCloudScatteringBoost(float boost) { cloud_scattering_boost_ = boost; }

			float GetCloudScatteringBoost() const { return cloud_scattering_boost_; }

			void SetCloudPowderStrength(float strength) { cloud_powder_strength_ = strength; }

			float GetCloudPowderStrength() const { return cloud_powder_strength_; }

			void  SetCloudCoverage(float coverage) { cloud_coverage_ = coverage; }
			float GetCloudCoverage() const { return cloud_coverage_; }

			void  SetCloudType(float type) { cloud_type_ = type; }
			float GetCloudType() const { return cloud_type_; }

			void  SetCloudWindSpeed(float speed) { cloud_wind_speed_ = speed; }
			float GetCloudWindSpeed() const { return cloud_wind_speed_; }

			void  SetCloudWindDir(const glm::vec3& dir) { cloud_wind_dir_ = dir; }
			glm::vec3 GetCloudWindDir() const { return cloud_wind_dir_; }

			void  SetCloudDetailScale(float scale) { cloud_detail_scale_ = scale; }
			float GetCloudDetailScale() const { return cloud_detail_scale_; }

			void  SetCloudCurlStrength(float strength) { cloud_curl_strength_ = strength; }
			float GetCloudCurlStrength() const { return cloud_curl_strength_; }

			GLuint GetTransmittanceLUT() const { return transmittance_lut_; }
			GLuint GetCloudNoiseLUT() const { return cloud_noise_lut_; }
			GLuint GetCloudDetailNoiseLUT() const { return cloud_detail_noise_lut_; }
			GLuint GetCurlNoiseLUT() const { return curl_noise_lut_; }
			GLuint GetWeatherMap() const { return weather_map_; }

		private:
			void GenerateLUTs();

			std::unique_ptr<Shader>        shader_;
			std::unique_ptr<ComputeShader> transmittance_lut_shader_;
			std::unique_ptr<ComputeShader> cloud_noise_lut_shader_;
			std::unique_ptr<ComputeShader> cloud_detail_noise_lut_shader_;
			std::unique_ptr<ComputeShader> curl_noise_lut_shader_;
			std::unique_ptr<ComputeShader> weather_map_shader_;

			GLuint transmittance_lut_ = 0;
			GLuint cloud_noise_lut_ = 0;
			GLuint cloud_detail_noise_lut_ = 0;
			GLuint curl_noise_lut_ = 0;
			GLuint weather_map_ = 0;

			float time_ = 0.0f;

			float     haze_density_ = 0.005f;
			float     haze_height_ = 20.0f;
			glm::vec3 haze_color_ = glm::vec3(0.6f, 0.7f, 0.8f);
			float     haze_g_ = 0.7f;

			float     cloud_density_ = 0.8f;
			float     cloud_altitude_ = 100.0f;
			float     cloud_thickness_ = 30.0f;
			glm::vec3 cloud_color_ = glm::vec3(0.95f, 0.95f, 1.0f);
			float     cloud_g_ = 0.8f;
			float     cloud_scattering_boost_ = 3.0f;
			float     cloud_powder_strength_ = 0.5f;

			float     cloud_coverage_ = 0.5f;
			float     cloud_type_ = 0.5f;
			float     cloud_wind_speed_ = 5.0f;
			glm::vec3 cloud_wind_dir_ = glm::vec3(1.0f, 0.0f, 0.5f);
			float     cloud_detail_scale_ = 1.0f;
			float     cloud_curl_strength_ = 0.5f;

			int width_ = 0;
			int height_ = 0;
		};

	} // namespace PostProcessing
} // namespace Boidsish
