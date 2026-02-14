#include "post_processing/effects/TimeStutterEffect.h"

#include "shader.h"
#include <GL/glew.h>

namespace Boidsish {
	namespace PostProcessing {

		TimeStutterEffect::TimeStutterEffect() {
			name_ = "TimeStutter";
			std::random_device rd;
			rng_ = std::mt19937(rd());
		}

		TimeStutterEffect::~TimeStutterEffect() {
			if (!history_texture_.empty()) {
				glDeleteTextures((GLsizei)history_texture_.size(), history_texture_.data());
				glDeleteFramebuffers((GLsizei)history_fbo_.size(), history_fbo_.data());
			}
		}

		void TimeStutterEffect::Initialize(int width, int height) {
			width_ = width;
			height_ = height;
			blit_shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/post_processing/passthrough.frag");
			InitializeFBOs(width, height);
		}

		void TimeStutterEffect::InitializeFBOs(int width, int height) {
			if (!history_texture_.empty()) {
				glDeleteTextures((GLsizei)history_texture_.size(), history_texture_.data());
				glDeleteFramebuffers((GLsizei)history_fbo_.size(), history_fbo_.data());
			}

			history_texture_.resize(kFrameHistoryCount);
			history_fbo_.resize(kFrameHistoryCount);

			glGenTextures(kFrameHistoryCount, history_texture_.data());
			glGenFramebuffers(kFrameHistoryCount, history_fbo_.data());

			for (int i = 0; i < kFrameHistoryCount; i++) {
				glBindFramebuffer(GL_FRAMEBUFFER, history_fbo_[i]);
				glBindTexture(GL_TEXTURE_2D, history_texture_[i]);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, NULL);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, history_texture_[i], 0);
			}
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		void TimeStutterEffect::Apply(const PostProcessingParams& params) {
			if (!blit_shader_) return;

			GLint originalFBO;
			glGetIntegerv(GL_FRAMEBUFFER_BINDING, &originalFBO);

			// 1. Store current frame in history
			glBindFramebuffer(GL_FRAMEBUFFER, history_fbo_[current_frame_idx_]);
			glViewport(0, 0, width_, height_);
			blit_shader_->use();
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, params.sourceTexture);
			blit_shader_->setInt("u_texture", 0);
			glDrawArrays(GL_TRIANGLES, 0, 6);

			// 2. Determine if we are stuttering
			if (time_ > stutter_end_time_) {
				std::uniform_real_distribution<float> chance(0, 1);
				if (chance(rng_) < 0.01f) { // 1% chance per frame
					std::uniform_real_distribution<float> duration(0.1f, 0.5f);
					stutter_end_time_ = time_ + duration(rng_);
					std::uniform_int_distribution<int> offset(1, kFrameHistoryCount - 1);
					displayed_frame_offset_ = offset(rng_);
				} else {
					displayed_frame_offset_ = 0;
				}
			}

			// 3. Render displayed frame (either current or historical)
			glBindFramebuffer(GL_FRAMEBUFFER, originalFBO);
			glViewport(0, 0, width_, height_);
			blit_shader_->use();
			glActiveTexture(GL_TEXTURE0);

			int idx = current_frame_idx_;
			if (displayed_frame_offset_ > 0) {
				idx = (current_frame_idx_ - displayed_frame_offset_ + kFrameHistoryCount) % kFrameHistoryCount;
			}
			glBindTexture(GL_TEXTURE_2D, history_texture_[idx]);
			blit_shader_->setInt("u_texture", 0);
			glDrawArrays(GL_TRIANGLES, 0, 6);

			current_frame_idx_ = (current_frame_idx_ + 1) % kFrameHistoryCount;
		}

		void TimeStutterEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
			InitializeFBOs(width, height);
		}

	} // namespace PostProcessing
} // namespace Boidsish
