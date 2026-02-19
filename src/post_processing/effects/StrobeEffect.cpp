#include "post_processing/effects/StrobeEffect.h"

#include "logger.h"
#include "shader.h"
#include <GL/glew.h>

namespace Boidsish {
	namespace PostProcessing {

		namespace {
			constexpr float kCaptureInterval = 0.1f;
			constexpr float kFadeDuration = 1.0f;
		} // namespace

		StrobeEffect::StrobeEffect() {
			name_ = "Strobe";
		}

		StrobeEffect::~StrobeEffect() {
			if (strobe_fbo_ != 0) {
				glDeleteFramebuffers(1, &strobe_fbo_);
				glDeleteTextures(1, &strobe_texture_);
			}
		}

		void StrobeEffect::Initialize(int width, int height) {
			shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/strobe.frag");
			blit_shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/postprocess.frag");
			width_ = width;
			height_ = height;
			InitializeFBO(width, height);
		}

		void StrobeEffect::InitializeFBO(int width, int height) {
			if (strobe_fbo_ != 0) {
				glDeleteFramebuffers(1, &strobe_fbo_);
				glDeleteTextures(1, &strobe_texture_);
			}

			glGenFramebuffers(1, &strobe_fbo_);
			glBindFramebuffer(GL_FRAMEBUFFER, strobe_fbo_);

			glGenTextures(1, &strobe_texture_);
			glBindTexture(GL_TEXTURE_2D, strobe_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, strobe_texture_, 0);

			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
				logger::ERROR("StrobeEffect FBO not complete!");
			}
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		void StrobeEffect::Apply(
			GLuint           sourceTexture,
			GLuint           depthTexture,
			GLuint           /* velocityTexture */,
			const glm::mat4& viewMatrix,
			const glm::mat4& projectionMatrix,
			const glm::vec3& cameraPos
		) {
			GLint previous_fbo;
			glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous_fbo);

			// Capture snapshot periodically into our internal buffer
			if (time_ - last_capture_time_ > kCaptureInterval) {
				glBindFramebuffer(GL_FRAMEBUFFER, strobe_fbo_);
				glViewport(0, 0, width_, height_);
				glClear(GL_COLOR_BUFFER_BIT);

				blit_shader_->use();
				blit_shader_->setInt("sceneTexture", 0);

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, sourceTexture);

				glDrawArrays(GL_TRIANGLES, 0, 6);

				last_capture_time_ = time_;
			}

			// Now, render the final output to the buffer that was originally bound.
			glBindFramebuffer(GL_FRAMEBUFFER, previous_fbo);
			glViewport(0, 0, width_, height_); // Set viewport for the destination

			// Apply the effect by blending the source texture and our strobe texture
			shader_->use();
			shader_->setInt("sceneTexture", 0);
			shader_->setInt("strobeTexture", 1);
			shader_->setFloat("time", time_);
			shader_->setFloat("lastCaptureTime", last_capture_time_);
			shader_->setFloat("fadeDuration", kFadeDuration);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, strobe_texture_);

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void StrobeEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
			InitializeFBO(width, height);
		}

	} // namespace PostProcessing
} // namespace Boidsish
