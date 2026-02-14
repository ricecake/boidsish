#include "post_processing/effects/StrobeEffect.h"

#include "shader.h"
#include <GL/glew.h>

namespace Boidsish {
	namespace PostProcessing {

		StrobeEffect::StrobeEffect() {
			name_ = "Strobe";
		}

		StrobeEffect::~StrobeEffect() {
			if (strobe_fbo_) glDeleteFramebuffers(1, &strobe_fbo_);
			if (strobe_texture_) glDeleteTextures(1, &strobe_texture_);
		}

		void StrobeEffect::Initialize(int width, int height) {
			shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/strobe.frag");
			blit_shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/post_processing/passthrough.frag");
			InitializeFBO(width, height);
		}

		void StrobeEffect::InitializeFBO(int width, int height) {
			width_ = width;
			height_ = height;

			if (strobe_fbo_) glDeleteFramebuffers(1, &strobe_fbo_);
			if (strobe_texture_) glDeleteTextures(1, &strobe_texture_);

			glGenFramebuffers(1, &strobe_fbo_);
			glBindFramebuffer(GL_FRAMEBUFFER, strobe_fbo_);
			glGenTextures(1, &strobe_texture_);
			glBindTexture(GL_TEXTURE_2D, strobe_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, strobe_texture_, 0);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		void StrobeEffect::Apply(const PostProcessingParams& params) {
			if (!shader_) return;

			// Logic to capture frame at intervals
			float interval = 0.1f;
			if (time_ - last_capture_time_ > interval) {
				GLint originalFBO;
				glGetIntegerv(GL_FRAMEBUFFER_BINDING, &originalFBO);
				glBindFramebuffer(GL_FRAMEBUFFER, strobe_fbo_);
				glViewport(0, 0, width_, height_);
				blit_shader_->use();
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, params.sourceTexture);
				blit_shader_->setInt("u_texture", 0);
				glDrawArrays(GL_TRIANGLES, 0, 6);
				glBindFramebuffer(GL_FRAMEBUFFER, originalFBO);
				last_capture_time_ = time_;
			}

			shader_->use();
			shader_->setInt("sceneTexture", 0);
			shader_->setInt("strobeTexture", 1);
			shader_->setFloat("time", time_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, params.sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, strobe_texture_);

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void StrobeEffect::Resize(int width, int height) {
			InitializeFBO(width, height);
		}

	} // namespace PostProcessing
} // namespace Boidsish
