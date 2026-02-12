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

			void Apply(const PostProcessingParams& params) override;
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

			// Cloud parameters
			void SetCloudDensity(float density) { cloud_density_ = density; }

			float GetCloudDensity() const { return cloud_density_; }

			void SetCloudAltitude(float altitude) { cloud_altitude_ = altitude; }

			float GetCloudAltitude() const { return cloud_altitude_; }

			void SetCloudThickness(float thickness) { cloud_thickness_ = thickness; }

			float GetCloudThickness() const { return cloud_thickness_; }

			void SetCloudColor(const glm::vec3& color) { cloud_color_ = color; }

			glm::vec3 GetCloudColor() const { return cloud_color_; }

		private:
			std::unique_ptr<Shader> shader_;
			float                   time_ = 0.0f;

			float     haze_density_ = 0.005f;
			float     haze_height_ = 20.0f;
			glm::vec3 haze_color_ = glm::vec3(0.6f, 0.7f, 0.8f);
			float     cloud_density_ = 0.5f;
			float     cloud_altitude_ = 95.0f;
			float     cloud_thickness_ = 10.0f;
			glm::vec3 cloud_color_ = glm::vec3(0.95f, 0.95f, 1.0f);

			int width_ = 0;
			int height_ = 0;
		};

	} // namespace PostProcessing
} // namespace Boidsish
