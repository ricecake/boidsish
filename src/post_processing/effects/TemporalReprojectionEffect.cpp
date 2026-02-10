#include "post_processing/effects/TemporalReprojectionEffect.h"
#include "logger.h"
#include "shader.h"
#include <GL/glew.h>

namespace Boidsish {
	namespace PostProcessing {

		TemporalReprojectionEffect::TemporalReprojectionEffect() {
			name_ = "TemporalReprojection";
			is_enabled_ = true;
			history_fbo_[0] = history_fbo_[1] = 0;
			history_texture_[0] = history_texture_[1] = 0;
		}

		TemporalReprojectionEffect::~TemporalReprojectionEffect() {
			if (history_fbo_[0])
				glDeleteFramebuffers(2, history_fbo_);
			if (history_texture_[0])
				glDeleteTextures(2, history_texture_);
		}

		void TemporalReprojectionEffect::Initialize(int width, int height) {
			width_ = width;
			height_ = height;
			shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/temporal_reprojection.frag");
			InitializeFBOs();
		}

		void TemporalReprojectionEffect::InitializeFBOs() {
			if (history_fbo_[0] != 0) {
				glDeleteFramebuffers(2, history_fbo_);
				glDeleteTextures(2, history_texture_);
			}

			glGenFramebuffers(2, history_fbo_);
			glGenTextures(2, history_texture_);

			for (int i = 0; i < 2; i++) {
				glBindFramebuffer(GL_FRAMEBUFFER, history_fbo_[i]);
				glBindTexture(GL_TEXTURE_2D, history_texture_[i]);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width_, height_, 0, GL_RGB, GL_FLOAT, NULL);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, history_texture_[i], 0);

				if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
					logger::ERROR("TemporalReprojection history FBO is not complete!");
			}
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		void TemporalReprojectionEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
			InitializeFBOs();
			first_frame_ = true;
		}

		void TemporalReprojectionEffect::Apply(const PostProcessingParams& params) {
			GLint originalFBO;
			glGetIntegerv(GL_FRAMEBUFFER_BINDING, &originalFBO);

			int read_idx = current_read_;
			int write_idx = 1 - current_read_;

			// 1. Accumulate into history
			glBindFramebuffer(GL_FRAMEBUFFER, history_fbo_[write_idx]);
			glViewport(0, 0, width_, height_);
			glClear(GL_COLOR_BUFFER_BIT);

			shader_->use();
			shader_->setInt("currentTexture", 0);
			shader_->setInt("historyTexture", 1);
			shader_->setInt("motionVectorTexture", 2);
			shader_->setBool("firstFrame", first_frame_);
			shader_->setBool("blitOnly", false);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, params.sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, history_texture_[read_idx]);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, params.motionVectorTexture);

			glDrawArrays(GL_TRIANGLES, 0, 6);

			// 2. Output to main chain (blit)
			glBindFramebuffer(GL_FRAMEBUFFER, originalFBO);
			shader_->setBool("blitOnly", true);
			glActiveTexture(GL_TEXTURE1); // Use historyTexture slot for blit
			glBindTexture(GL_TEXTURE_2D, history_texture_[write_idx]);

			glDrawArrays(GL_TRIANGLES, 0, 6);

			current_read_ = write_idx;
			first_frame_ = false;
		}

	} // namespace PostProcessing
} // namespace Boidsish
