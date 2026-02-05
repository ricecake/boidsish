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
		}

		void AtmosphereEffect::Initialize(int width, int height) {
			shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/atmosphere.frag");
			transmittance_lut_shader_ = std::make_unique<ComputeShader>("shaders/helpers/atmosphere_lut.comp");
			cloud_noise_lut_shader_ = std::make_unique<ComputeShader>("shaders/helpers/cloud_noise_lut.comp");

			GLuint lighting_idx = glGetUniformBlockIndex(shader_->ID, "Lighting");
			if (lighting_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(shader_->ID, lighting_idx, 0);
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
			shader_->setInt("transmittanceLUT", 2);
			shader_->setInt("cloudNoiseLUT", 3);
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

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, depthTexture);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, transmittance_lut_);
			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_3D, cloud_noise_lut_);

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void AtmosphereEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
		}

	} // namespace PostProcessing
} // namespace Boidsish
