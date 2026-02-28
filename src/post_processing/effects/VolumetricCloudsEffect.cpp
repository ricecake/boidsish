#include "post_processing/effects/VolumetricCloudsEffect.h"

#include <iostream>

#include "constants.h"
#include "shader.h"

namespace Boidsish {
	namespace PostProcessing {

		VolumetricCloudsEffect::VolumetricCloudsEffect() {
			name_ = "VolumetricClouds";
		}

		VolumetricCloudsEffect::~VolumetricCloudsEffect() {
			if (history_fbo_[0] != 0) {
				glDeleteFramebuffers(2, history_fbo_);
				glDeleteTextures(2, history_texture_);
			}
			if (low_res_fbo_ != 0) {
				glDeleteFramebuffers(1, &low_res_fbo_);
				glDeleteTextures(1, &low_res_cloud_texture_);
			}
		}

		void VolumetricCloudsEffect::Initialize(int width, int height) {
			shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/volumetric_clouds.frag");
			upsample_shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/cloud_upsample.frag");

			shader_->use();
			GLuint lighting_idx = glGetUniformBlockIndex(shader_->ID, "Lighting");
			if (lighting_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(shader_->ID, lighting_idx, Constants::UboBinding::Lighting());
			}
			GLuint temporal_idx = glGetUniformBlockIndex(shader_->ID, "TemporalData");
			if (temporal_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(shader_->ID, temporal_idx, Constants::UboBinding::TemporalData());
			}

			width_ = width;
			height_ = height;
			low_res_width_ = width / 2;
			low_res_height_ = height / 2;

			CreateBuffers();
		}

		void VolumetricCloudsEffect::CreateBuffers() {
			if (history_fbo_[0] != 0) {
				glDeleteFramebuffers(2, history_fbo_);
				glDeleteTextures(2, history_texture_);
			}
			if (low_res_fbo_ != 0) {
				glDeleteFramebuffers(1, &low_res_fbo_);
				glDeleteTextures(1, &low_res_cloud_texture_);
			}

			glGenFramebuffers(2, history_fbo_);
			glGenTextures(2, history_texture_);

			for (int i = 0; i < 2; i++) {
				glBindFramebuffer(GL_FRAMEBUFFER, history_fbo_[i]);
				glBindTexture(GL_TEXTURE_2D, history_texture_[i]);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, low_res_width_, low_res_height_, 0, GL_RGBA, GL_FLOAT, nullptr);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, history_texture_[i], 0);

				if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
					std::cerr << "ERROR::FRAMEBUFFER:: Cloud history FBO is not complete!" << std::endl;
			}

			glGenFramebuffers(1, &low_res_fbo_);
			glGenTextures(1, &low_res_cloud_texture_);
			glBindFramebuffer(GL_FRAMEBUFFER, low_res_fbo_);
			glBindTexture(GL_TEXTURE_2D, low_res_cloud_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, low_res_width_, low_res_height_, 0, GL_RGBA, GL_FLOAT, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, low_res_cloud_texture_, 0);

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		void VolumetricCloudsEffect::Apply(
			GLuint sourceTexture,
			GLuint depthTexture,
			GLuint /* velocityTexture */,
			const glm::mat4& /* viewMatrix */,
			const glm::mat4& /* projectionMatrix */,
			const glm::vec3& /* cameraPos */
		) {
			// Save current draw FBO and viewport
			GLint previous_fbo;
			glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous_fbo);
			GLint viewport[4];
			glGetIntegerv(GL_VIEWPORT, viewport);

			// 1. Raymarch at low resolution directly into history (which accumulates)
			glBindFramebuffer(GL_FRAMEBUFFER, history_fbo_[current_history_]);
			glViewport(0, 0, low_res_width_, low_res_height_);
			shader_->use();
			shader_->setInt("depthTexture", 1);
			shader_->setInt("cloudBaseNoise", 2);
			shader_->setInt("cloudDetailNoise", 3);
			shader_->setInt("weatherMap", 4);
			shader_->setInt("curlNoise", 5);
			shader_->setInt("historyTexture", 6);

			shader_->setFloat("u_cloudHeight", cloud_height_);
			shader_->setFloat("u_cloudThickness", cloud_thickness_);
			shader_->setFloat("u_cloudDensity", cloud_density_);
			shader_->setFloat("u_cloudCoverage", cloud_coverage_);
			shader_->setFloat("u_cloudWarp", cloud_warp_);
			shader_->setFloat("u_cloudType", cloud_type_);
			shader_->setFloat("u_warpPush", warp_push_);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, depthTexture);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_3D, cloud_base_noise_);
			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_3D, cloud_detail_noise_);
			glActiveTexture(GL_TEXTURE4);
			glBindTexture(GL_TEXTURE_2D, weather_map_);
			glActiveTexture(GL_TEXTURE5);
			glBindTexture(GL_TEXTURE_3D, curl_noise_);
			glActiveTexture(GL_TEXTURE6);
			glBindTexture(GL_TEXTURE_2D, history_texture_[1 - current_history_]);

			glDrawArrays(GL_TRIANGLES, 0, 6);

			// 2. Final composite at full resolution into the original target FBO
			glBindFramebuffer(GL_FRAMEBUFFER, previous_fbo);
			glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
			upsample_shader_->use();
			upsample_shader_->setInt("sceneTexture", 0);
			upsample_shader_->setInt("cloudTexture", 1);
			upsample_shader_->setInt("depthTexture", 2);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, history_texture_[current_history_]);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, depthTexture);

			glDrawArrays(GL_TRIANGLES, 0, 6);

			current_history_ = 1 - current_history_;
		}

		void VolumetricCloudsEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
			low_res_width_ = width / 2;
			low_res_height_ = height / 2;
			CreateBuffers();
		}

	} // namespace PostProcessing
} // namespace Boidsish
