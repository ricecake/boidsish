#include "post_processing/effects/WhispTrailEffect.h"

#include "shader.h"
#include <GL/glew.h>

namespace Boidsish {
	namespace PostProcessing {

		WhispTrailEffect::WhispTrailEffect() {
			name_ = "WhispTrail";
		}

		WhispTrailEffect::~WhispTrailEffect() {
			if (trail_texture_[0])
				glDeleteTextures(2, trail_texture_);
			if (trail_fbo_[0])
				glDeleteFramebuffers(2, trail_fbo_);
		}

		void WhispTrailEffect::Initialize(int width, int height) {
			width_ = width;
			height_ = height;
			trail_shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/whisp_trail.frag");
			blit_shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/post_processing/passthrough.frag");
			InitializeFBOs(width, height);
		}

		void WhispTrailEffect::InitializeFBOs(int width, int height) {
			if (trail_texture_[0]) {
				glDeleteTextures(2, trail_texture_);
				glDeleteFramebuffers(2, trail_fbo_);
			}

			glGenFramebuffers(2, trail_fbo_);
			glGenTextures(2, trail_texture_);

			for (int i = 0; i < 2; i++) {
				glBindFramebuffer(GL_FRAMEBUFFER, trail_fbo_[i]);
				glBindTexture(GL_TEXTURE_2D, trail_texture_[i]);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, NULL);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, trail_texture_[i], 0);
			}
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		void WhispTrailEffect::Apply(const PostProcessingParams& params) {
			if (!trail_shader_) return;

			GLint originalFBO;
			glGetIntegerv(GL_FRAMEBUFFER_BINDING, &originalFBO);

			int read_idx = current_read_;
			int write_idx = 1 - current_read_;

			// 1. Accumulate trails
			glBindFramebuffer(GL_FRAMEBUFFER, trail_fbo_[write_idx]);
			glViewport(0, 0, width_, height_);

			trail_shader_->use();
			trail_shader_->setInt("currentTexture", 0);
			trail_shader_->setInt("historyTexture", 1);
			trail_shader_->setFloat("time", time_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, params.sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, trail_texture_[read_idx]);

			glDrawArrays(GL_TRIANGLES, 0, 6);

			// 2. Composite result back to output
			glBindFramebuffer(GL_FRAMEBUFFER, originalFBO);
			glViewport(0, 0, width_, height_);
			blit_shader_->use();
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, trail_texture_[write_idx]);
			blit_shader_->setInt("u_texture", 0);
			glDrawArrays(GL_TRIANGLES, 0, 6);

			current_read_ = write_idx;
		}

		void WhispTrailEffect::Resize(int width, int height) {
			InitializeFBOs(width, height);
		}

	} // namespace PostProcessing
} // namespace Boidsish
