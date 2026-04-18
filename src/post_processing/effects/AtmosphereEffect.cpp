#include "post_processing/effects/AtmosphereEffect.h"

#include "constants.h"
#include "shader.h"

namespace Boidsish {
	namespace PostProcessing {

		AtmosphereEffect::AtmosphereEffect() {
			name_ = "Atmosphere";
		}

		AtmosphereEffect::~AtmosphereEffect() {
			if (low_res_fbo_) {
				glDeleteFramebuffers(1, &low_res_fbo_);
				glDeleteTextures(1, &low_res_texture_);
			}
			if (temporal_textures_[0]) {
				glDeleteTextures(2, temporal_textures_);
			}
		}

		void AtmosphereEffect::Initialize(int width, int height) {
			shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/atmosphere_lowres.frag");
			composite_shader_ = std::make_unique<Shader>(
				"shaders/postprocess.vert",
				"shaders/effects/atmosphere_composite.frag"
			);
			temporal_shader_ = std::make_unique<ComputeShader>("shaders/effects/cloud_temporal_reprojection.comp");

			auto setup_shader = [](Shader& s) {
				s.use();
				GLuint lighting_idx = glGetUniformBlockIndex(s.ID, "Lighting");
				if (lighting_idx != GL_INVALID_INDEX) {
					glUniformBlockBinding(s.ID, lighting_idx, Constants::UboBinding::Lighting());
				}
				GLuint shadows_idx = glGetUniformBlockIndex(s.ID, "Shadows");
				if (shadows_idx != GL_INVALID_INDEX) {
					glUniformBlockBinding(s.ID, shadows_idx, Constants::UboBinding::Shadows());
				}
				GLuint effects_idx = glGetUniformBlockIndex(s.ID, "VisualEffects");
				if (effects_idx != GL_INVALID_INDEX) {
					glUniformBlockBinding(s.ID, effects_idx, Constants::UboBinding::VisualEffects());
				}

				GLuint vol_idx = glGetUniformBlockIndex(s.ID, "VolumetricLighting");
				if (vol_idx != GL_INVALID_INDEX) {
					glUniformBlockBinding(s.ID, vol_idx, Constants::UboBinding::VolumetricLighting());
				}

				// Explicitly set standard sampler bindings
				s.setInt("shadowMaps", 4);
			};

			setup_shader(*shader_);
			setup_shader(*composite_shader_);

			width_ = width;
			height_ = height;

			InitializeLowResResources();
			InitializeTemporalResources();
		}

		void AtmosphereEffect::InitializeLowResResources() {
			if (low_res_fbo_ == 0) {
				glGenFramebuffers(1, &low_res_fbo_);
				glGenTextures(1, &low_res_texture_);
			}

			int low_res_width = std::max(1, static_cast<int>(width_ * render_scale_));
			int low_res_height = std::max(1, static_cast<int>(height_ * render_scale_));

			glBindFramebuffer(GL_FRAMEBUFFER, low_res_fbo_);
			glBindTexture(GL_TEXTURE_2D, low_res_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, low_res_width, low_res_height, 0, GL_RGBA, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, low_res_texture_, 0);

			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
				std::cerr << "ERROR::ATMOSPHERE_EFFECT: Low-res FBO is not complete!" << std::endl;
			}

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		void AtmosphereEffect::InitializeTemporalResources() {
			int low_res_width = std::max(1, static_cast<int>(width_ * render_scale_));
			int low_res_height = std::max(1, static_cast<int>(height_ * render_scale_));

			if (temporal_textures_[0]) {
				glDeleteTextures(2, temporal_textures_);
			}
			glGenTextures(2, temporal_textures_);
			for (int i = 0; i < 2; i++) {
				glBindTexture(GL_TEXTURE_2D, temporal_textures_[i]);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, low_res_width, low_res_height, 0, GL_RGBA, GL_FLOAT, NULL);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			}
			glBindTexture(GL_TEXTURE_2D, 0);
			has_valid_history_ = false;
		}

		void AtmosphereEffect::Apply(GLuint sourceTexture, GLuint depthTexture, GLuint velocityTexture, GLuint normalTexture, GLuint albedoTexture, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraPos) {
			// Re-bind the previous framebuffer (which was the target for this effect)
			// We MUST bind it back if we changed it.
			// Save the current FBO before changing it.
			GLint original_fbo;
			glGetIntegerv(GL_FRAMEBUFFER_BINDING, &original_fbo);

			// --- PASS 1: Low-res Cloud Rendering ---
			int low_res_width = std::max(1, static_cast<int>(width_ * render_scale_));
			int low_res_height = std::max(1, static_cast<int>(height_ * render_scale_));

			glBindFramebuffer(GL_FRAMEBUFFER, low_res_fbo_);
			glViewport(0, 0, low_res_width, low_res_height);
			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			glClear(GL_COLOR_BUFFER_BIT);

			shader_->use();
			shader_->setInt("depthTexture", 1);
			shader_->setFloat("time", time_);
			shader_->setMat4("invView", glm::inverse(viewMatrix));
			shader_->setMat4("invProjection", glm::inverse(projectionMatrix));

			shader_->setFloat("cloudDensity", cloud_density_);
			shader_->setFloat("cloudAltitude", cloud_altitude_);
			shader_->setFloat("cloudThickness", cloud_thickness_);
			shader_->setFloat("cloudCoverage", cloud_coverage_);
			shader_->setFloat("cloudWarp", cloud_warp_);
			shader_->setVec3("cloudColorUniform", cloud_color_);
			shader_->setFloat("u_atmosphereHeight", atmosphere_height_);

			shader_->setInt("u_transmittanceLUT", 20);
			shader_->setInt("u_skyViewLUT", 22);
			shader_->trySetInt("u_noiseTexture", 5);
			shader_->trySetInt("u_curlTexture", 6);
			shader_->trySetInt("u_blueNoiseTexture", 7);
			shader_->trySetInt("u_extraNoiseTexture", 8);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, depthTexture);
			glActiveTexture(GL_TEXTURE20);
			glBindTexture(GL_TEXTURE_2D, transmittance_lut_);
			glActiveTexture(GL_TEXTURE22);
			glBindTexture(GL_TEXTURE_2D, sky_view_lut_);
			glActiveTexture(GL_TEXTURE5);
			glBindTexture(GL_TEXTURE_3D, noise_textures_.noise);
			glActiveTexture(GL_TEXTURE6);
			glBindTexture(GL_TEXTURE_3D, noise_textures_.curl);
			glActiveTexture(GL_TEXTURE7);
			glBindTexture(GL_TEXTURE_2D, noise_textures_.blue_noise);
			glActiveTexture(GL_TEXTURE8);
			glBindTexture(GL_TEXTURE_3D, noise_textures_.extra_noise);

			glDrawArrays(GL_TRIANGLES, 0, 6);

			// --- PASS 1.5: Temporal Reprojection (compute, at low res) ---
			// Blend current low-res clouds with reprojected history to reduce noise and
			// effectively supersample the low-res buffer over multiple frames.
			GLuint    cloud_source = low_res_texture_;
			glm::mat4 invView = glm::inverse(viewMatrix);
			glm::mat4 invProj = glm::inverse(projectionMatrix);

			if (temporal_shader_ && temporal_shader_->isValid()) {
				int next_temporal = 1 - temporal_index_;

				temporal_shader_->use();
				temporal_shader_->setFloat("uBlendAlpha", has_valid_history_ ? 0.75f : 0.0f);
				temporal_shader_->setInt("uFrameIndex", frame_index_);
				temporal_shader_->setMat4("uInvView", invView);
				temporal_shader_->setMat4("uInvProjection", invProj);
				temporal_shader_->setMat4("uPrevViewProjection", prev_view_projection_);

				temporal_shader_->setInt("uCurrentFrame", 0);
				temporal_shader_->setInt("uHistoryFrame", 1);
				temporal_shader_->setInt("uDepthTexture", 2);

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, low_res_texture_);
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, temporal_textures_[temporal_index_]);
				glActiveTexture(GL_TEXTURE2);
				glBindTexture(GL_TEXTURE_2D, depthTexture);

				glBindImageTexture(0, temporal_textures_[next_temporal], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

				glDispatchCompute((low_res_width + 7) / 8, (low_res_height + 7) / 8, 1);
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

				temporal_index_ = next_temporal;
				has_valid_history_ = true;
				cloud_source = temporal_textures_[temporal_index_];
			}

			// Store current VP for next frame's reprojection
			prev_view_projection_ = projectionMatrix * viewMatrix;
			frame_index_++;

			// --- PASS 2: High-res Atmosphere Composition with bilateral upsample ---
			glBindFramebuffer(GL_FRAMEBUFFER, original_fbo);
			glViewport(0, 0, width_, height_);

			composite_shader_->use();
			composite_shader_->setInt("sceneTexture", 0);
			composite_shader_->setInt("depthTexture", 1);
			composite_shader_->setInt("cloudTexture", 2);
			composite_shader_->setFloat("time", time_);
			composite_shader_->setMat4("invView", invView);
			composite_shader_->setMat4("invProjection", invProj);

			composite_shader_->setFloat("hazeDensity", haze_density_);
			composite_shader_->setFloat("hazeHeight", haze_height_);
			composite_shader_->setVec3("hazeColor", haze_color_);

			composite_shader_->setVec2("cloudTexelSize", glm::vec2(1.0f / low_res_width, 1.0f / low_res_height));
			composite_shader_->setFloat("u_atmosphereHeight", atmosphere_height_);

			composite_shader_->setInt("u_transmittanceLUT", 20);
			composite_shader_->setInt("u_aerialPerspectiveLUT", 23);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, depthTexture);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, cloud_source);

			glActiveTexture(GL_TEXTURE20);
			glBindTexture(GL_TEXTURE_2D, transmittance_lut_);
			glActiveTexture(GL_TEXTURE23);
			glBindTexture(GL_TEXTURE_3D, aerial_perspective_lut_);

			// Bind Volumetric Cascades (Units 28, 29, 30)
			glActiveTexture(GL_TEXTURE28);
			glBindTexture(GL_TEXTURE_3D, volumetric_textures_[0]);
			composite_shader_->trySetInt("u_volumetricIntegrated0", 28);

			glActiveTexture(GL_TEXTURE29);
			glBindTexture(GL_TEXTURE_3D, volumetric_textures_[1]);
			composite_shader_->trySetInt("u_volumetricIntegrated1", 29);

			glActiveTexture(GL_TEXTURE30);
			glBindTexture(GL_TEXTURE_3D, volumetric_textures_[2]);
			composite_shader_->trySetInt("u_volumetricIntegrated2", 30);

			glDrawArrays(GL_TRIANGLES, 0, 6);

			// Cleanup
			glActiveTexture(GL_TEXTURE23);
			glBindTexture(GL_TEXTURE_3D, 0);
			glActiveTexture(GL_TEXTURE20);
			glBindTexture(GL_TEXTURE_2D, 0);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, 0);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, 0);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		void AtmosphereEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
			InitializeLowResResources();
			InitializeTemporalResources();
		}

	} // namespace PostProcessing
} // namespace Boidsish
