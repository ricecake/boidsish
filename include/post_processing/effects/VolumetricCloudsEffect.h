#pragma once

#include <memory>
#include <vector>

#include "post_processing/IPostProcessingEffect.h"

class Shader;

namespace Boidsish {
	namespace PostProcessing {

		class VolumetricCloudsEffect: public IPostProcessingEffect {
		public:
			VolumetricCloudsEffect();
			~VolumetricCloudsEffect();

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

			// Cloud parameters
			void SetCloudHeight(float h) { cloud_height_ = h; }

			float GetCloudHeight() const { return cloud_height_; }

			void SetCloudThickness(float t) { cloud_thickness_ = t; }

			float GetCloudThickness() const { return cloud_thickness_; }

			void SetCloudDensity(float d) { cloud_density_ = d; }

			float GetCloudDensity() const { return cloud_density_; }

			void SetCloudCoverage(float c) { cloud_coverage_ = c; }

			float GetCloudCoverage() const { return cloud_coverage_; }

			void SetCloudWarp(float w) { cloud_warp_ = w; }

			float GetCloudWarp() const { return cloud_warp_; }

			void SetCloudType(float t) { cloud_type_ = t; }

			float GetCloudType() const { return cloud_type_; }

			void SetWarpPush(float p) { warp_push_ = p; }

			float GetWarpPush() const { return warp_push_; }

			// Noise textures
			void SetNoiseTextures(GLuint base, GLuint detail, GLuint weather, GLuint curl) {
				cloud_base_noise_ = base;
				cloud_detail_noise_ = detail;
				weather_map_ = weather;
				curl_noise_ = curl;
			}

		private:
			void CreateBuffers();
			void ClearBuffers();

			std::unique_ptr<Shader> shader_;
			std::unique_ptr<Shader> upsample_shader_;
			float                   time_ = 0.0f;

			float cloud_height_ = 1200.0f;
			float cloud_thickness_ = 500.0f;
			float cloud_density_ = 0.8f;
			float cloud_coverage_ = 0.6f;
			float cloud_warp_ = 1.0f;
			float cloud_type_ = 0.5f;
			float warp_push_ = 0.5f;

			GLuint cloud_base_noise_ = 0;
			GLuint cloud_detail_noise_ = 0;
			GLuint weather_map_ = 0;
			GLuint curl_noise_ = 0;
			GLuint blue_noise_ = 0;

			GLuint low_res_fbo_ = 0;
			GLuint low_res_cloud_texture_ = 0;

			GLuint history_fbo_[2] = {0, 0};
			GLuint history_texture_[2] = {0, 0};
			int    current_history_ = 0;

			int width_ = 0;
			int height_ = 0;
			int low_res_width_ = 0;
			int low_res_height_ = 0;
		};

	} // namespace PostProcessing
} // namespace Boidsish
