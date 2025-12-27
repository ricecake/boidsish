#include "post_processing/PostProcessingManager.h"

#include <iostream>

#include <shader.h>

namespace Boidsish {
	namespace PostProcessing {

		PostProcessingManager::PostProcessingManager(int width, int height, GLuint quad_vao):
			width_(width), height_(height), quad_vao_(quad_vao) {
			pingpong_fbo_[0] = 0;
			pingpong_fbo_[1] = 0;
		}

		PostProcessingManager::~PostProcessingManager() {
			glDeleteFramebuffers(2, pingpong_fbo_);
			glDeleteTextures(2, pingpong_texture_);
		}

		void PostProcessingManager::Initialize() {
			InitializeFBOs();
		}

		void PostProcessingManager::InitializeFBOs() {
			// Clean up existing resources if any
			if (pingpong_fbo_[0] != 0) {
				glDeleteFramebuffers(2, pingpong_fbo_);
				glDeleteTextures(2, pingpong_texture_);
			}

			glGenFramebuffers(2, pingpong_fbo_);
			glGenTextures(2, pingpong_texture_);

			for (unsigned int i = 0; i < 2; i++) {
				glBindFramebuffer(GL_FRAMEBUFFER, pingpong_fbo_[i]);
				glBindTexture(GL_TEXTURE_2D, pingpong_texture_[i]);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width_, height_, 0, GL_RGB, GL_FLOAT, NULL);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpong_texture_[i], 0);
				if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
					std::cerr << "ERROR::FRAMEBUFFER:: Ping-pong FBO is not complete!" << std::endl;
			}
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		void PostProcessingManager::AddEffect(std::shared_ptr<IPostProcessingEffect> effect) {
			effect->Initialize(width_, height_);
			effects_.push_back(effect);
		}

		GLuint PostProcessingManager::ApplyEffects(GLuint sourceTexture) {
			bool   first_pass = true;
			int    fbo_index = 0;
			GLuint current_texture = sourceTexture;

			for (const auto& effect : effects_) {
				if (effect->IsEnabled()) {
					glBindFramebuffer(GL_FRAMEBUFFER, pingpong_fbo_[fbo_index]);
					glClear(GL_COLOR_BUFFER_BIT); // Clear the buffer before drawing

					glBindVertexArray(quad_vao_);
					effect->Apply(current_texture);
					glBindVertexArray(0);

					current_texture = pingpong_texture_[fbo_index];
					fbo_index = 1 - fbo_index; // Flip index
					first_pass = false;
				}
			}

			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			// If no effects were enabled, we just return the original scene texture
			if (first_pass) {
				return sourceTexture;
			}

			// The final texture is in the FBO we are NOT about to render to.
			return pingpong_texture_[1 - fbo_index];
		}

		void PostProcessingManager::Render() {
			glBindVertexArray(quad_vao_);
			glDrawArrays(GL_TRIANGLES, 0, 6);
			glBindVertexArray(0);
		}

		void PostProcessingManager::Resize(int width, int height) {
			width_ = width;
			height_ = height;
			InitializeFBOs();

			for (const auto& effect : effects_) {
				effect->Resize(width, height);
			}
		}

	} // namespace PostProcessing
} // namespace Boidsish
