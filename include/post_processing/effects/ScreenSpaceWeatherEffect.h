#pragma once

#include <memory>
#include "post_processing/IPostProcessingEffect.h"

class Shader;

namespace Boidsish {
	namespace PostProcessing {

		class ScreenSpaceWeatherEffect : public IPostProcessingEffect {
		public:
			ScreenSpaceWeatherEffect();
			~ScreenSpaceWeatherEffect();

			void Initialize(int width, int height) override;
			void Apply(GLuint sourceTexture, GLuint depthTexture, GLuint velocityTexture, GLuint normalTexture, GLuint albedoTexture, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraPos) override;
			void Resize(int width, int height) override;

			bool IsEarly() const override { return true; }

			void SetTime(float time) override { time_ = time; }

			// Getters/setters for controls
			float GetHeatStrength() const { return heat_strength_; }
			void SetHeatStrength(float v) { heat_strength_ = v; }

			float GetHeatScale() const { return heat_scale_; }
			void SetHeatScale(float v) { heat_scale_ = v; }

			float GetHeatSpeed() const { return heat_speed_; }
			void SetHeatSpeed(float v) { heat_speed_ = v; }

			float GetWindAngle() const { return wind_angle_; }
			void SetWindAngle(float v) { wind_angle_ = v; }

			float GetWindSpeed() const { return wind_speed_; }
			void SetWindSpeed(float v) { wind_speed_ = v; }

			float GetWindBlurScale() const { return wind_blur_scale_; }
			void SetWindBlurScale(float v) { wind_blur_scale_ = v; }

			float GetWindGustFrequency() const { return wind_gust_frequency_; }
			void SetWindGustFrequency(float v) { wind_gust_frequency_ = v; }

			float GetWindGustStrength() const { return wind_gust_strength_; }
			void SetWindGustStrength(float v) { wind_gust_strength_ = v; }

			bool IsManualIceCoverage() const { return use_manual_ice_coverage_; }
			void SetManualIceCoverage(bool b) { use_manual_ice_coverage_ = b; }

			float GetIceCoverage() const { return ice_coverage_; }
			void SetIceCoverage(float v) { ice_coverage_ = v; }

			float GetIceScale() const { return ice_scale_; }
			void SetIceScale(float v) { ice_scale_ = v; }

			float GetIceEdgeWidth() const { return ice_edge_width_; }
			void SetIceEdgeWidth(float v) { ice_edge_width_ = v; }

			glm::vec3 GetIceColor() const { return ice_color_; }
			void SetIceColor(const glm::vec3& c) { ice_color_ = c; }

		private:
			std::unique_ptr<Shader> shader_;
			float                   time_ = 0.0f;

			// Controls and parameters
			float     heat_strength_ = 0.15f;
			float     heat_scale_ = 1.0f;
			float     heat_speed_ = 1.5f;

			float     wind_angle_ = 0.0f;
			float     wind_speed_ = 3.0f;
			float     wind_blur_scale_ = 0.015f;
			float     wind_gust_frequency_ = 2.0f;
			float     wind_gust_strength_ = 0.5f;

			bool      use_manual_ice_coverage_ = false;
			float     ice_coverage_ = 0.0f;
			float     ice_scale_ = 5.0f;
			float     ice_edge_width_ = 0.3f;
			glm::vec3 ice_color_ = glm::vec3(0.85f, 0.95f, 1.0f);
		};

	} // namespace PostProcessing
} // namespace Boidsish
