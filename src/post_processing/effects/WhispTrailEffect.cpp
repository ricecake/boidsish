#include "post_processing/effects/WhispTrailEffect.h"

#include "logger.h"
#include "shader.h"
#include <GL/glew.h>

namespace Boidsish {
	namespace PostProcessing {

		WhispTrailEffect::WhispTrailEffect(): current_read_(0), width_(0), height_(0), time_(0.0f) {
			name_ = "Whisp Trail";
			// Zero-initialize handles
			trail_fbo_[0] = trail_fbo_[1] = 0;
			trail_texture_[0] = trail_texture_[1] = 0;
		}

		WhispTrailEffect::~WhispTrailEffect() {
			glDeleteFramebuffers(2, trail_fbo_);
			glDeleteTextures(2, trail_texture_);
		}

		void WhispTrailEffect::Initialize(int width, int height) {
			trail_shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/whisp_trail.frag");
			// A simple pass-through shader to blit textures
			blit_shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/postprocess.frag");

			width_ = width;
			height_ = height;
			InitializeFBOs(width, height);
		}

		void WhispTrailEffect::InitializeFBOs(int width, int height) {
			glDeleteFramebuffers(2, trail_fbo_);
			glDeleteTextures(2, trail_texture_);

			glGenFramebuffers(2, trail_fbo_);
			glGenTextures(2, trail_texture_);

			for (int i = 0; i < 2; ++i) {
				glBindFramebuffer(GL_FRAMEBUFFER, trail_fbo_[i]);
				glBindTexture(GL_TEXTURE_2D, trail_texture_[i]);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, trail_texture_[i], 0);

				if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
					logger::ERROR("WhispTrailEffect FBO not complete!");
				}
			}
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		void WhispTrailEffect::Apply(GLuint sourceTexture, float /* delta_time */) {
			// Save original state
			GLint previous_fbo;
			glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous_fbo);
			GLint viewport[4];
			glGetIntegerv(GL_VIEWPORT, viewport);

			int read_idx = current_read_;
			int write_idx = 1 - current_read_;

			// 1. Process the previous frame's trail into our internal FBO
			glBindFramebuffer(GL_FRAMEBUFFER, trail_fbo_[write_idx]);
			glViewport(0, 0, width_, height_);
			glClear(GL_COLOR_BUFFER_BIT);

			trail_shader_->use();
			trail_shader_->setInt("sceneTexture", 0);
			trail_shader_->setInt("trailTexture", 1);
			trail_shader_->setFloat("time", time_);
			trail_shader_->setVec2("resolution", (float)width_, (float)height_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture); // The current scene
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, trail_texture_[read_idx]); // The previous frame's trail

			glDrawArrays(GL_TRIANGLES, 0, 6);

			// 2. Blit the result to the main output (the manager's FBO)
			glBindFramebuffer(GL_FRAMEBUFFER, previous_fbo);
			glViewport(viewport[0], viewport[1], viewport[2], viewport[3]); // Restore viewport

			blit_shader_->use();
			blit_shader_->setInt("sceneTexture", 0);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, trail_texture_[write_idx]);

			glDrawArrays(GL_TRIANGLES, 0, 6);

			// Swap buffers for the next frame
			current_read_ = write_idx;
		}

		void WhispTrailEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
			InitializeFBOs(width, height);
		}

	} // namespace PostProcessing
} // namespace Boidsish
