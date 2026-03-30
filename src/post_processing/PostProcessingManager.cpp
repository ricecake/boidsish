#include "post_processing/PostProcessingManager.h"

#include <iostream>

#include "profiler.h"
#include <shader.h>

namespace Boidsish {
	namespace PostProcessing {

		PostProcessingManager::PostProcessingManager(int width, int height, GLuint quad_vao):
			width_(width), height_(height), quad_vao_(quad_vao) {
		}

		PostProcessingManager::~PostProcessingManager() {
			for (auto& pair : scaled_buffers_) {
				glDeleteFramebuffers(2, pair.second.fbo);
				glDeleteTextures(2, pair.second.texture);
			}
		}

		void PostProcessingManager::Initialize() {
			InitializeFBOs();
			passthrough_shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/postprocess.frag");
		}

		void PostProcessingManager::InitializeFBOs() {
			// Clean up existing resources if any
			for (auto& pair : scaled_buffers_) {
				glDeleteFramebuffers(2, pair.second.fbo);
				glDeleteTextures(2, pair.second.texture);
			}
			scaled_buffers_.clear();

			InitializeScaledBuffer(1.0f);
		}

		void PostProcessingManager::InitializeScaledBuffer(float scale) {
			if (scaled_buffers_.find(scale) != scaled_buffers_.end()) {
				return;
			}

			PingPongBuffer buffer;
			buffer.width = static_cast<int>(width_ * scale);
			buffer.height = static_cast<int>(height_ * scale);
			buffer.fbo_index = 0;

			glGenFramebuffers(2, buffer.fbo);
			glGenTextures(2, buffer.texture);

			for (unsigned int i = 0; i < 2; i++) {
				glBindFramebuffer(GL_FRAMEBUFFER, buffer.fbo[i]);
				glBindTexture(GL_TEXTURE_2D, buffer.texture[i]);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, buffer.width, buffer.height, 0, GL_RGB, GL_FLOAT, NULL);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, buffer.texture[i], 0);

				if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
					std::cerr << "ERROR::FRAMEBUFFER:: Ping-pong FBO (scale " << scale << ") is not complete!" << std::endl;
			}
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			scaled_buffers_[scale] = buffer;
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
			current_scale_ = 1.0f;
			glViewport(0, 0, width_, height_);

			DetachDepthFromPingPongFBOs();
		}

		void PostProcessingManager::AttachDepthToCurrentFBO() {
			if (shared_depth_texture_ == 0)
				return;

			// Only attach to our ping-pong FBOs, not the source FBO (which should already have its own depth)
			// Need to check if current_fbo_ is in any of our scaled buffers
			bool is_pingpong = false;
			for (const auto& pair : scaled_buffers_) {
				if (current_fbo_ == pair.second.fbo[0] || current_fbo_ == pair.second.fbo[1]) {
					is_pingpong = true;
					break;
				}
			}

			if (is_pingpong) {
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
			for (auto& pair : scaled_buffers_) {
				for (int i = 0; i < 2; ++i) {
					glBindFramebuffer(GL_FRAMEBUFFER, pair.second.fbo[i]);
					glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
				}
			}
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		void PostProcessingManager::ApplyEarlyEffects(
			const glm::mat4& viewMatrix,
			const glm::mat4& projectionMatrix,
			const glm::vec3& cameraPos,
			float            time
		) {
			PROJECT_PROFILE_SCOPE("ApplyEarlyEffects");
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
			PROJECT_PROFILE_SCOPE("ApplyLateEffects");
			for (const auto& effect : pre_tone_mapping_effects_) {
				if (effect->IsEnabled() && !effect->IsEarly()) {
					ApplyEffectInternal(effect, viewMatrix, projectionMatrix, cameraPos, time);
				}
			}

			if (tone_mapping_effect_ && tone_mapping_effect_->IsEnabled()) {
				ApplyEffectInternal(tone_mapping_effect_, viewMatrix, projectionMatrix, cameraPos, time);
			}

			// Ensure we are at full resolution before finishing
			EnsureFullRes();

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		void PostProcessingManager::SetNightFactor(float factor) {
			for (auto& effect : pre_tone_mapping_effects_) {
				effect->SetNightFactor(factor);
			}
			if (tone_mapping_effect_) {
				tone_mapping_effect_->SetNightFactor(factor);
			}
		}

		void PostProcessingManager::EnsureFullRes() {
			if (current_scale_ == 1.0f) {
				return;
			}

			PROJECT_PROFILE_SCOPE("PostProcess::EnsureFullRes");

			float target_scale = 1.0f;
			InitializeScaledBuffer(target_scale);
			auto& buffer = scaled_buffers_[target_scale];

			glBindFramebuffer(GL_FRAMEBUFFER, buffer.fbo[buffer.fbo_index]);
			glViewport(0, 0, buffer.width, buffer.height);
			glClear(GL_COLOR_BUFFER_BIT);

			glDisable(GL_DEPTH_TEST);
			glDepthMask(GL_FALSE);

			passthrough_shader_->use();
			passthrough_shader_->setInt("sceneTexture", 0);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, current_texture_);

			glBindVertexArray(quad_vao_);
			glDrawArrays(GL_TRIANGLES, 0, 6);
			glBindVertexArray(0);

			glEnable(GL_DEPTH_TEST);
			glDepthMask(GL_TRUE);

			current_texture_ = buffer.texture[buffer.fbo_index];
			current_fbo_ = buffer.fbo[buffer.fbo_index];
			buffer.fbo_index = 1 - buffer.fbo_index;
			current_scale_ = target_scale;
		}

		void PostProcessingManager::ApplyEffectInternal(
			std::shared_ptr<IPostProcessingEffect> effect,
			const glm::mat4&                       viewMatrix,
			const glm::mat4&                       projectionMatrix,
			const glm::vec3&                       cameraPos,
			float                                  time
		) {
			float target_scale = effect->GetRenderScale();
			InitializeScaledBuffer(target_scale);
			auto& buffer = scaled_buffers_[target_scale];

			effect->SetTime(time);
			glBindFramebuffer(GL_FRAMEBUFFER, buffer.fbo[buffer.fbo_index]);
			glViewport(0, 0, buffer.width, buffer.height);
			glClear(GL_COLOR_BUFFER_BIT);

			// Post-processing quads should not be depth-tested or write to depth buffer
			glDisable(GL_DEPTH_TEST);
			glDepthMask(GL_FALSE);

			glBindVertexArray(quad_vao_);
			effect->Apply(current_texture_, depth_texture_, velocity_texture_, viewMatrix, projectionMatrix, cameraPos);
			glBindVertexArray(0);

			glEnable(GL_DEPTH_TEST);
			glDepthMask(GL_TRUE);

			current_texture_ = buffer.texture[buffer.fbo_index];
			current_fbo_ = buffer.fbo[buffer.fbo_index];
			buffer.fbo_index = 1 - buffer.fbo_index;
			current_scale_ = target_scale;
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
