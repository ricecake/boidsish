#include "post_processing/effects/AtmosphereEffect.h"

#include "shader.h"

namespace Boidsish {
	namespace PostProcessing {

		AtmosphereEffect::AtmosphereEffect() {
			name_ = "Atmosphere";
		}

		AtmosphereEffect::~AtmosphereEffect() {
			if (transmittance_lut_ != 0) {
				glDeleteTextures(1, &transmittance_lut_);
			}
			if (cloud_noise_lut_ != 0) {
				glDeleteTextures(1, &cloud_noise_lut_);
			}
			if (cloud_detail_noise_lut_ != 0) {
				glDeleteTextures(1, &cloud_detail_noise_lut_);
			}
			if (curl_noise_lut_ != 0) {
				glDeleteTextures(1, &curl_noise_lut_);
			}
			if (weather_map_ != 0) {
				glDeleteTextures(1, &weather_map_);
			}
		}

		void AtmosphereEffect::Initialize(int width, int height) {
			shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/atmosphere.frag");
			transmittance_lut_shader_ = std::make_unique<ComputeShader>("shaders/helpers/atmosphere_lut.comp");
			cloud_noise_lut_shader_ = std::make_unique<ComputeShader>("shaders/helpers/cloud_noise_lut.comp");
			cloud_detail_noise_lut_shader_ =
				std::make_unique<ComputeShader>("shaders/helpers/cloud_detail_noise_lut.comp");
			curl_noise_lut_shader_ = std::make_unique<ComputeShader>("shaders/helpers/curl_noise_lut.comp");
			weather_map_shader_ = std::make_unique<ComputeShader>("shaders/helpers/weather_map.comp");

			// Set up UBO bindings for atmosphere shader
			GLuint lighting_idx = glGetUniformBlockIndex(shader_->ID, "Lighting");
			if (lighting_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(shader_->ID, lighting_idx, 0);
			}
			GLuint shadows_idx = glGetUniformBlockIndex(shader_->ID, "Shadows");
			if (shadows_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(shader_->ID, shadows_idx, 2);
			} else {
				// Shadows not found - maybe it was optimized out?
				// This shouldn't happen if lighting.glsl is included.
			}
			GLuint effects_idx = glGetUniformBlockIndex(shader_->ID, "VisualEffects");
			if (effects_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(shader_->ID, effects_idx, 1);
			}
			GLuint frustum_idx = glGetUniformBlockIndex(shader_->ID, "FrustumData");
			if (frustum_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(shader_->ID, frustum_idx, 3);
			}

			width_ = width;
			height_ = height;

			GenerateLUTs();
		}

		void AtmosphereEffect::GenerateLUTs() {
			// 1. Transmittance LUT (2D)
			if (transmittance_lut_ == 0) {
				glGenTextures(1, &transmittance_lut_);
			}

			glBindTexture(GL_TEXTURE_2D, transmittance_lut_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 256, 64, 0, GL_RGBA, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			if (transmittance_lut_shader_ && transmittance_lut_shader_->isValid()) {
				transmittance_lut_shader_->use();
				glBindImageTexture(0, transmittance_lut_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
				transmittance_lut_shader_->dispatch(16, 4, 1); // 16*16 = 256, 4*16 = 64
			}

			// 2. Cloud Noise LUT (3D)
			if (cloud_noise_lut_ == 0) {
				glGenTextures(1, &cloud_noise_lut_);
			}

			glBindTexture(GL_TEXTURE_3D, cloud_noise_lut_);
			glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 128, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);

			if (cloud_noise_lut_shader_ && cloud_noise_lut_shader_->isValid()) {
				cloud_noise_lut_shader_->use();
				glBindImageTexture(1, cloud_noise_lut_, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA8);
				cloud_noise_lut_shader_->dispatch(16, 16, 16); // 16*8 = 128
			}

			// 3. Cloud Detail Noise LUT (3D)
			if (cloud_detail_noise_lut_ == 0) {
				glGenTextures(1, &cloud_detail_noise_lut_);
			}

			glBindTexture(GL_TEXTURE_3D, cloud_detail_noise_lut_);
			glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 32, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);

			if (cloud_detail_noise_lut_shader_ && cloud_detail_noise_lut_shader_->isValid()) {
				cloud_detail_noise_lut_shader_->use();
				glBindImageTexture(2, cloud_detail_noise_lut_, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA8);
				cloud_detail_noise_lut_shader_->dispatch(4, 4, 4); // 4*8 = 32
			}

			// 4. Curl Noise LUT (2D)
			if (curl_noise_lut_ == 0) {
				glGenTextures(1, &curl_noise_lut_);
			}

			glBindTexture(GL_TEXTURE_2D, curl_noise_lut_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

			if (curl_noise_lut_shader_ && curl_noise_lut_shader_->isValid()) {
				curl_noise_lut_shader_->use();
				glBindImageTexture(3, curl_noise_lut_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
				curl_noise_lut_shader_->dispatch(8, 8, 1); // 8*16 = 128
			}

			// 5. Weather Map (2D)
			if (weather_map_ == 0) {
				glGenTextures(1, &weather_map_);
			}

			glBindTexture(GL_TEXTURE_2D, weather_map_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1024, 1024, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

			if (weather_map_shader_ && weather_map_shader_->isValid()) {
				weather_map_shader_->use();
				glBindImageTexture(4, weather_map_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
				weather_map_shader_->dispatch(64, 64, 1); // 64*16 = 1024
			}

			glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
		}

		void AtmosphereEffect::Apply(
			GLuint           sourceTexture,
			GLuint           depthTexture,
			const glm::mat4& viewMatrix,
			const glm::mat4& projectionMatrix,
			const glm::vec3& cameraPos
		) {
			shader_->use();
			shader_->setInt("sceneTexture", 0);
			shader_->setInt("depthTexture", 1);
			shader_->setInt("transmittanceLUT", 10);
			shader_->setInt("cloudNoiseLUT", 11);
			shader_->setInt("cloudDetailNoiseLUT", 12);
			shader_->setInt("curlNoiseLUT", 13);
			shader_->setInt("weatherMap", 14);

			shader_->setFloat("time", time_);
			shader_->setVec3("cameraPos", cameraPos);
			shader_->setMat4("invView", glm::inverse(viewMatrix));
			shader_->setMat4("invProjection", glm::inverse(projectionMatrix));

			shader_->setFloat("hazeDensity", haze_density_);
			shader_->setFloat("hazeHeight", haze_height_);
			shader_->setVec3("hazeColor", haze_color_);
			shader_->setFloat("cloudDensity", cloud_density_);
			shader_->setFloat("cloudAltitude", cloud_altitude_);
			shader_->setFloat("cloudThickness", cloud_thickness_);
			shader_->setVec3("cloudColorUniform", cloud_color_);

			// New enhanced parameters
			shader_->setFloat("hazeG", haze_g_);
			shader_->setFloat("cloudG", cloud_g_);
			shader_->setFloat("cloudScatteringBoost", cloud_scattering_boost_);
			shader_->setFloat("cloudPowderStrength", cloud_powder_strength_);

			// HZD specific parameters
			shader_->setFloat("cloudCoverage", cloud_coverage_);
			shader_->setFloat("cloudType", cloud_type_);
			shader_->setFloat("cloudWindSpeed", cloud_wind_speed_);
			shader_->setVec3("cloudWindDir", cloud_wind_dir_);
			shader_->setFloat("cloudDetailScale", cloud_detail_scale_);
			shader_->setFloat("cloudCurlStrength", cloud_curl_strength_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, depthTexture);
			glActiveTexture(GL_TEXTURE10);
			glBindTexture(GL_TEXTURE_2D, transmittance_lut_);
			glActiveTexture(GL_TEXTURE11);
			glBindTexture(GL_TEXTURE_3D, cloud_noise_lut_);
			glActiveTexture(GL_TEXTURE12);
			glBindTexture(GL_TEXTURE_3D, cloud_detail_noise_lut_);
			glActiveTexture(GL_TEXTURE13);
			glBindTexture(GL_TEXTURE_2D, curl_noise_lut_);
			glActiveTexture(GL_TEXTURE14);
			glBindTexture(GL_TEXTURE_2D, weather_map_);

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void AtmosphereEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
		}

	} // namespace PostProcessing
} // namespace Boidsish
