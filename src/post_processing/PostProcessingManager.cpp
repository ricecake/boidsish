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
			pre_tone_mapping_effects_.push_back(effect);
		}

		void PostProcessingManager::SetToneMappingEffect(std::shared_ptr<IPostProcessingEffect> effect) {
			if (effect) {
				effect->Initialize(width_, height_);
			}
			tone_mapping_effect_ = effect;
		}

		void PostProcessingManager::StartFrame(GLuint sourceTexture, GLuint depthTexture, float time) {
			current_texture_ = sourceTexture;
			depth_texture_ = depthTexture;
			time_ = time;
			fbo_index_ = 0;
			effect_applied_ = false;

			// Attach depth texture to ping-pong FBOs for depth testing in mid-pass
			for (int i = 0; i < 2; i++) {
				glBindFramebuffer(GL_FRAMEBUFFER, pingpong_fbo_[i]);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depth_texture_, 0);
			}

			// Ensure the viewport is set correctly for our FBOs before starting
			glViewport(0, 0, width_, height_);
		}

		void PostProcessingManager::ApplyPreTransparencyEffects(
			const glm::mat4& viewMatrix,
			const glm::mat4& projectionMatrix,
			const glm::vec3& cameraPos
		) {
			for (const auto& effect : pre_tone_mapping_effects_) {
				if (effect->IsEnabled() && effect->IsPreTransparency()) {
					effect->SetTime(time_);
					glBindFramebuffer(GL_FRAMEBUFFER, pingpong_fbo_[fbo_index_]);
					glClear(GL_COLOR_BUFFER_BIT);

					glBindVertexArray(quad_vao_);
					effect->Apply(current_texture_, depth_texture_, viewMatrix, projectionMatrix, cameraPos);
					glBindVertexArray(0);

					current_texture_ = pingpong_texture_[fbo_index_];
					fbo_index_ = 1 - fbo_index_;
					effect_applied_ = true;
				}
			}
		}

		void PostProcessingManager::ApplyPostTransparencyEffects(
			const glm::mat4& viewMatrix,
			const glm::mat4& projectionMatrix,
			const glm::vec3& cameraPos
		) {
			// Apply remaining pre-tone-mapping effects
			for (const auto& effect : pre_tone_mapping_effects_) {
				if (effect->IsEnabled() && !effect->IsPreTransparency()) {
					effect->SetTime(time_);
					glBindFramebuffer(GL_FRAMEBUFFER, pingpong_fbo_[fbo_index_]);
					glClear(GL_COLOR_BUFFER_BIT);

					glBindVertexArray(quad_vao_);
					effect->Apply(current_texture_, depth_texture_, viewMatrix, projectionMatrix, cameraPos);
					glBindVertexArray(0);

					current_texture_ = pingpong_texture_[fbo_index_];
					fbo_index_ = 1 - fbo_index_;
					effect_applied_ = true;
				}
			}

			// Apply the tone mapping effect as the final step
			if (tone_mapping_effect_ && tone_mapping_effect_->IsEnabled()) {
				tone_mapping_effect_->SetTime(time_);
				glBindFramebuffer(GL_FRAMEBUFFER, pingpong_fbo_[fbo_index_]);
				glClear(GL_COLOR_BUFFER_BIT);

				glBindVertexArray(quad_vao_);
				tone_mapping_effect_->Apply(current_texture_, depth_texture_, viewMatrix, projectionMatrix, cameraPos);
				glBindVertexArray(0);

				current_texture_ = pingpong_texture_[fbo_index_];
				effect_applied_ = true;
			}

			// Restore the default framebuffer
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		GLuint PostProcessingManager::ApplyEffects(
			GLuint           sourceTexture,
			GLuint           depthTexture,
			const glm::mat4& viewMatrix,
			const glm::mat4& projectionMatrix,
			const glm::vec3& cameraPos,
			float            time
		) {
			StartFrame(sourceTexture, depthTexture, time);
			ApplyPreTransparencyEffects(viewMatrix, projectionMatrix, cameraPos);
			ApplyPostTransparencyEffects(viewMatrix, projectionMatrix, cameraPos);

			return GetCurrentResult();
		}

		void PostProcessingManager::Resize(int width, int height) {
			width_ = width;
			height_ = height;
			InitializeFBOs();

			for (const auto& effect : pre_tone_mapping_effects_) {
				effect->Resize(width, height);
			}

			if (tone_mapping_effect_) {
				tone_mapping_effect_->Resize(width, height);
			}
		}

	} // namespace PostProcessing
} // namespace Boidsish
