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

		void PostProcessingManager::SetSharedDepthTexture(GLuint texture) {
			shared_depth_texture_ = texture;
			InitializeFBOs();
		}

		void PostProcessingManager::BeginApply(
			GLuint sourceTexture,
			GLuint sourceFbo,
			GLuint depthTexture,
			GLuint velocityTexture
		) {
			current_texture_ = sourceTexture;
			current_fbo_ = sourceFbo;
			depth_texture_ = depthTexture;
			velocity_texture_ = velocityTexture;
			fbo_index_ = 0;
			glViewport(0, 0, width_, height_);

			DetachDepthFromPingPongFBOs();
		}

		void PostProcessingManager::AttachDepthToCurrentFBO() {
			if (shared_depth_texture_ == 0)
				return;

			// Only attach to our ping-pong FBOs, not the source FBO (which should already have its own depth)
			if (current_fbo_ == pingpong_fbo_[0] || current_fbo_ == pingpong_fbo_[1]) {
				glBindFramebuffer(GL_FRAMEBUFFER, current_fbo_);
				glFramebufferTexture2D(
					GL_FRAMEBUFFER,
					GL_DEPTH_STENCIL_ATTACHMENT,
					GL_TEXTURE_2D,
					shared_depth_texture_,
					0
				);
			}
		}

		void PostProcessingManager::DetachDepthFromPingPongFBOs() {
			for (int i = 0; i < 2; ++i) {
				glBindFramebuffer(GL_FRAMEBUFFER, pingpong_fbo_[i]);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
			}
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		void PostProcessingManager::ApplyEarlyEffects(
			const glm::mat4& viewMatrix,
			const glm::mat4& projectionMatrix,
			const glm::vec3& cameraPos,
			float            time
		) {
			DetachDepthFromPingPongFBOs();

			for (const auto& effect : pre_tone_mapping_effects_) {
				if (effect->IsEnabled() && effect->IsEarly()) {
					ApplyEffectInternal(effect, viewMatrix, projectionMatrix, cameraPos, time);
				}
			}
		}

		void PostProcessingManager::ApplyLateEffects(
			const glm::mat4& viewMatrix,
			const glm::mat4& projectionMatrix,
			const glm::vec3& cameraPos,
			float            time
		) {
			for (const auto& effect : pre_tone_mapping_effects_) {
				if (effect->IsEnabled() && !effect->IsEarly()) {
					ApplyEffectInternal(effect, viewMatrix, projectionMatrix, cameraPos, time);
				}
			}

			if (tone_mapping_effect_ && tone_mapping_effect_->IsEnabled()) {
				ApplyEffectInternal(tone_mapping_effect_, viewMatrix, projectionMatrix, cameraPos, time);
			}

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		void PostProcessingManager::ApplyEffectInternal(
			std::shared_ptr<IPostProcessingEffect> effect,
			const glm::mat4&                       viewMatrix,
			const glm::mat4&                       projectionMatrix,
			const glm::vec3&                       cameraPos,
			float                                  time
		) {
			effect->SetTime(time);
			glBindFramebuffer(GL_FRAMEBUFFER, pingpong_fbo_[fbo_index_]);
			glClear(GL_COLOR_BUFFER_BIT);

			// Post-processing quads should not be depth-tested or write to depth buffer
			glDisable(GL_DEPTH_TEST);
			glDepthMask(GL_FALSE);

			glBindVertexArray(quad_vao_);
			effect->Apply(current_texture_, depth_texture_, velocity_texture_, viewMatrix, projectionMatrix, cameraPos);
			glBindVertexArray(0);

			glEnable(GL_DEPTH_TEST);
			glDepthMask(GL_TRUE);

			current_texture_ = pingpong_texture_[fbo_index_];
			current_fbo_ = pingpong_fbo_[fbo_index_];
			fbo_index_ = 1 - fbo_index_;
		}

		GLuint PostProcessingManager::ApplyEffects(
			GLuint           sourceTexture,
			GLuint           depthTexture,
			const glm::mat4& viewMatrix,
			const glm::mat4& projectionMatrix,
			const glm::vec3& cameraPos,
			float            time
		) {
			BeginApply(sourceTexture, 0, depthTexture, 0); // Deprecated call, passing 0 for velocity
			ApplyEarlyEffects(viewMatrix, projectionMatrix, cameraPos, time);
			ApplyLateEffects(viewMatrix, projectionMatrix, cameraPos, time);
			return GetFinalTexture();
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
