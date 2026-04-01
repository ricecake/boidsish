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
		}

		void AtmosphereEffect::Initialize(int width, int height) {
			shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/atmosphere_lowres.frag");
			composite_shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/atmosphere_composite.frag");

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

				// Explicitly set standard sampler bindings
				s.setInt("shadowMaps", 4);
			};

			setup_shader(*shader_);
			setup_shader(*composite_shader_);

			width_ = width;
			height_ = height;

			InitializeLowResResources();
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

		void AtmosphereEffect::Apply(
			GLuint sourceTexture,
			GLuint depthTexture,
			GLuint /* velocityTexture */,
			const glm::mat4& viewMatrix,
			const glm::mat4& projectionMatrix,
			const glm::vec3& /* cameraPos */
		) {
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

			shader_->setInt("u_skyViewLUT", 12);
			shader_->trySetInt("u_noiseTexture", 5);
			shader_->trySetInt("u_curlTexture", 6);
			shader_->trySetInt("u_blueNoiseTexture", 7);
			shader_->trySetInt("u_extraNoiseTexture", 8);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, depthTexture);
			glActiveTexture(GL_TEXTURE5);
			glBindTexture(GL_TEXTURE_3D, noise_textures_.noise);
			glActiveTexture(GL_TEXTURE6);
			glBindTexture(GL_TEXTURE_3D, noise_textures_.curl);
			glActiveTexture(GL_TEXTURE7);
			glBindTexture(GL_TEXTURE_2D, noise_textures_.blue_noise);
			glActiveTexture(GL_TEXTURE8);
			glBindTexture(GL_TEXTURE_3D, noise_textures_.extra_noise);
			glActiveTexture(GL_TEXTURE12);
			glBindTexture(GL_TEXTURE_2D, sky_view_lut_);

			glDrawArrays(GL_TRIANGLES, 0, 6);

			// --- PASS 2: High-res Atmosphere Composition ---
			// Note: The caller (PostProcessingManager) will re-bind its ping-pong FBO after our Apply() returns,
			// but we need to bind a target to draw into right now.
			// However, since AtmosphereEffect is a PostProcessingEffect, the current FBO context
			// is already set to the ping-pong target by PostProcessingManager::ApplyEffectInternal
			// before calling Apply().
			// We MUST bind it back if we changed it.
			// But ApplyEffectInternal binds pingpong_fbo_[fbo_index_], we don't know which one it is.
			// Actually, the easiest way is to let PostProcessingManager handle the binding,
			// but we need two shaders in one Apply.

			glBindFramebuffer(GL_FRAMEBUFFER, original_fbo);
			glViewport(0, 0, width_, height_);

			composite_shader_->use();
			composite_shader_->setInt("sceneTexture", 0);
			composite_shader_->setInt("depthTexture", 1);
			composite_shader_->setInt("cloudTexture", 2);
			composite_shader_->setFloat("time", time_);
			composite_shader_->setMat4("invView", glm::inverse(viewMatrix));
			composite_shader_->setMat4("invProjection", glm::inverse(projectionMatrix));

			composite_shader_->setFloat("hazeDensity", haze_density_);
			composite_shader_->setFloat("hazeHeight", haze_height_);
			composite_shader_->setVec3("hazeColor", haze_color_);

			composite_shader_->setInt("u_transmittanceLUT", 10);
			composite_shader_->setInt("u_skyViewLUT", 12);
			composite_shader_->setInt("u_aerialPerspectiveLUT", 13);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, depthTexture);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, low_res_texture_);

			glActiveTexture(GL_TEXTURE10);
			glBindTexture(GL_TEXTURE_2D, transmittance_lut_);
			glActiveTexture(GL_TEXTURE12);
			glBindTexture(GL_TEXTURE_2D, sky_view_lut_);
			glActiveTexture(GL_TEXTURE13);
			glBindTexture(GL_TEXTURE_3D, aerial_perspective_lut_);

			glDrawArrays(GL_TRIANGLES, 0, 6);

			// Cleanup
			glActiveTexture(GL_TEXTURE13); glBindTexture(GL_TEXTURE_3D, 0);
			glActiveTexture(GL_TEXTURE12); glBindTexture(GL_TEXTURE_2D, 0);
			glActiveTexture(GL_TEXTURE10); glBindTexture(GL_TEXTURE_2D, 0);
			glActiveTexture(GL_TEXTURE2);  glBindTexture(GL_TEXTURE_2D, 0);
			glActiveTexture(GL_TEXTURE1);  glBindTexture(GL_TEXTURE_2D, 0);
			glActiveTexture(GL_TEXTURE0);  glBindTexture(GL_TEXTURE_2D, 0);
		}

		void AtmosphereEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
			InitializeLowResResources();
		}

	} // namespace PostProcessing
} // namespace Boidsish
