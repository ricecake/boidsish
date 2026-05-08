#include "post_processing/effects/PulseEffect.h"

#include "constants.h"
#include "logger.h"
#include "shader.h"
#include <GL/glew.h>

namespace Boidsish {
	namespace PostProcessing {

		PulseEffect::PulseEffect() {
			name_ = "Pulse";
		}

		PulseEffect::~PulseEffect() {
			if (pulse_fbo_ != 0) {
				glDeleteFramebuffers(1, &pulse_fbo_);
				glDeleteTextures(1, &pulse_texture_);
			}
		}

		void PulseEffect::Initialize(int width, int height) {
			width_ = width;
			height_ = height;

			ray_shader_ = std::make_unique<Shader>("shaders/effects/pulse_ray.vert", "shaders/effects/pulse_ray.frag");
			composite_shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/pulse_composite.frag");

			if (ray_shader_->isValid()) {
				GLuint temporal_idx = glGetUniformBlockIndex(ray_shader_->ID, "TemporalData");
				if (temporal_idx != GL_INVALID_INDEX) glUniformBlockBinding(ray_shader_->ID, temporal_idx, Constants::UboBinding::TemporalData());
			}

			InitializeFBO(width, height);
		}

		void PulseEffect::InitializeFBO(int width, int height) {
			if (pulse_fbo_ != 0) {
				glDeleteFramebuffers(1, &pulse_fbo_);
				glDeleteTextures(1, &pulse_texture_);
			}

			glGenFramebuffers(1, &pulse_fbo_);
			glBindFramebuffer(GL_FRAMEBUFFER, pulse_fbo_);

			glGenTextures(1, &pulse_texture_);
			glBindTexture(GL_TEXTURE_2D, pulse_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, width, height, 0, GL_RED, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pulse_texture_, 0);

			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
				logger::ERROR("PulseEffect FBO not complete!");
			}
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		void PulseEffect::Apply(GLuint sourceTexture, GLuint depthTexture, GLuint velocityTexture, GLuint normalTexture, GLuint albedoTexture, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraPos) {
			float elapsed = time_ - last_trigger_time_;
			if (elapsed > pulse_duration_) {
				is_pulsing_ = false;
			}

			GLint previous_fbo;
			glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous_fbo);

			// 1. Render pulses to accumulation texture
			glBindFramebuffer(GL_FRAMEBUFFER, pulse_fbo_);
			glViewport(0, 0, width_, height_);
			glClearColor(0, 0, 0, 0);
			glClear(GL_COLOR_BUFFER_BIT);

			if (is_pulsing_) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_ONE, GL_ONE);
				glDisable(GL_DEPTH_TEST);
				glDepthMask(GL_FALSE);

				ray_shader_->use();
				ray_shader_->setVec3("uPulseOrigin", pulse_origin_);
				ray_shader_->setFloat("uCurrentRadius", elapsed * speed_);
				ray_shader_->setFloat("uMaxRadius", pulse_duration_ * speed_);
				ray_shader_->setInt("uMaxBounces", max_bounces_);
				ray_shader_->setMat4("uView", viewMatrix);
				ray_shader_->setMat4("uProj", projectionMatrix);
				ray_shader_->setVec3("uCameraPos", cameraPos);

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, depthTexture);
				ray_shader_->setInt("uDepthTexture", 0);

				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, normalTexture);
				ray_shader_->setInt("uNormalTexture", 1);

				// Bind an empty VAO for procedural vertex generation
				static GLuint empty_vao = 0;
				if (empty_vao == 0) glGenVertexArrays(1, &empty_vao);
				glBindVertexArray(empty_vao);

				glDrawArrays(GL_POINTS, 0, 100000);

				glDisable(GL_BLEND);
				glDepthMask(GL_TRUE);
			}

			// 2. Composite with scene
			glBindFramebuffer(GL_FRAMEBUFFER, previous_fbo);
			glViewport(0, 0, width_, height_);

			composite_shader_->use();
			composite_shader_->setInt("uSceneTexture", 0);
			composite_shader_->setInt("uPulseTexture", 1);
			composite_shader_->setFloat("uBrightness", brightness_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, pulse_texture_);

			// PostProcessingManager already bound quad_vao_
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void PulseEffect::Trigger(const glm::vec3& origin) {
			pulse_origin_ = origin;
			last_trigger_time_ = time_;
			is_pulsing_ = true;
			logger::INFO("Pulse triggered at (%.2f, %.2f, %.2f)", origin.x, origin.y, origin.z);
		}

		void PulseEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
			InitializeFBO(width, height);
		}

	} // namespace PostProcessing
} // namespace Boidsish
