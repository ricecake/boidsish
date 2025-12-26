#include "post_processing/PostProcessingManager.h"
#include "shader.h"
#include <iostream>

namespace Boidsish {
	namespace PostProcessing {

		PostProcessingManager::PostProcessingManager(int width, int height):
			width_(width), height_(height), quad_vao_(0), quad_vbo_(0) {}

		PostProcessingManager::~PostProcessingManager() {
			glDeleteFramebuffers(2, pingpong_fbo_);
			glDeleteTextures(2, pingpong_texture_);
			glDeleteVertexArrays(1, &quad_vao_);
			glDeleteBuffers(1, &quad_vbo_);
		}

		void PostProcessingManager::Initialize() {
			InitializeFBOs();
			InitializeQuad();
		}

		void PostProcessingManager::InitializeQuad() {
			float quad_vertices[] = {
				-1.0f, 1.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f,

				-1.0f, 1.0f, 0.0f, 1.0f, 1.0f,  -1.0f, 1.0f, 0.0f, 1.0f, 1.0f,  1.0f, 1.0f
			};
			glGenVertexArrays(1, &quad_vao_);
			glBindVertexArray(quad_vao_);
			glGenBuffers(1, &quad_vbo_);
			glBindBuffer(GL_ARRAY_BUFFER, quad_vbo_);
			glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
			glEnableVertexAttribArray(1);
			glBindVertexArray(0);
		}

		void PostProcessingManager::InitializeFBOs() {
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
					std::cerr << "ERROR::FRAMEBUFFER:: Ping-pong Framebuffer is not complete!" << std::endl;
			}
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		void PostProcessingManager::AddEffect(std::shared_ptr<IPostProcessingEffect> effect) {
			effect->Initialize(width_, height_);
			effects_.push_back(effect);
		}

		GLuint PostProcessingManager::ApplyEffects(GLuint sourceTexture) {
			bool horizontal = true;
			bool first_iteration = true;

			for (const auto& effect : effects_) {
				if (effect->IsEnabled()) {
					glBindFramebuffer(GL_FRAMEBUFFER, pingpong_fbo_[horizontal]);
					effect->Apply(first_iteration ? sourceTexture : pingpong_texture_[!horizontal]);

					glBindVertexArray(quad_vao_);
					glDrawArrays(GL_TRIANGLES, 0, 6);

					horizontal = !horizontal;
					if (first_iteration) {
						first_iteration = false;
					}
				}
			}

			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			return first_iteration ? sourceTexture : pingpong_texture_[!horizontal];
		}

		void PostProcessingManager::Resize(int width, int height) {
			width_ = width;
			height_ = height;

			for (unsigned int i = 0; i < 2; i++) {
				glBindTexture(GL_TEXTURE_2D, pingpong_texture_[i]);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width_, height_, 0, GL_RGB, GL_FLOAT, NULL);
			}
			glBindTexture(GL_TEXTURE_2D, 0);

			for (const auto& effect : effects_) {
				effect->Resize(width, height);
			}
		}

	} // namespace PostProcessing
} // namespace Boidsish
