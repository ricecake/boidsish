#include "post_processing/PostProcessingManager.h"

#include <algorithm>
#include <iostream>

#include <shader.h>

namespace Boidsish {
	namespace PostProcessing {

		PostProcessingManager::PostProcessingManager(int width, int height, GLuint quad_vao):
			width_(width), height_(height), quad_vao_(quad_vao) {
			pingpong_fbo_[0] = 0;
			pingpong_fbo_[1] = 0;
			hiz_shader_ = std::make_unique<ComputeShader>("shaders/effects/hiz_depth.comp");
		}

		PostProcessingManager::~PostProcessingManager() {
			glDeleteFramebuffers(2, pingpong_fbo_);
			glDeleteTextures(2, pingpong_texture_);
			if (motion_vector_fbo_)
				glDeleteFramebuffers(1, &motion_vector_fbo_);
			if (motion_vector_texture_)
				glDeleteTextures(1, &motion_vector_texture_);
			if (hiz_texture_)
				glDeleteTextures(1, &hiz_texture_);
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
			if (motion_vector_fbo_) {
				glDeleteFramebuffers(1, &motion_vector_fbo_);
				glDeleteTextures(1, &motion_vector_texture_);
			}
			if (hiz_texture_) {
				glDeleteTextures(1, &hiz_texture_);
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

			// Motion Vector FBO
			glGenFramebuffers(1, &motion_vector_fbo_);
			glBindFramebuffer(GL_FRAMEBUFFER, motion_vector_fbo_);
			glGenTextures(1, &motion_vector_texture_);
			glBindTexture(GL_TEXTURE_2D, motion_vector_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, width_, height_, 0, GL_RG, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, motion_vector_texture_, 0);
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				std::cerr << "ERROR::FRAMEBUFFER:: Motion Vector FBO is not complete!" << std::endl;

			// Hi-Z Texture
			glGenTextures(1, &hiz_texture_);
			glBindTexture(GL_TEXTURE_2D, hiz_texture_);
			int levels = (int)std::floor(std::log2(std::max(width_, height_))) + 1;
			glTexStorage2D(GL_TEXTURE_2D, levels, GL_R32F, width_, height_);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		void PostProcessingManager::AddEffect(std::shared_ptr<IPostProcessingEffect> effect) {
			effect->Initialize(width_, height_);
			pre_tone_mapping_effects_.push_back(effect);
		}

		void PostProcessingManager::SetMotionVectorEffect(std::shared_ptr<IPostProcessingEffect> effect) {
			if (effect) {
				effect->Initialize(width_, height_);
			}
			motion_vector_effect_ = effect;
		}

		void PostProcessingManager::SetToneMappingEffect(std::shared_ptr<IPostProcessingEffect> effect) {
			if (effect) {
				effect->Initialize(width_, height_);
			}
			tone_mapping_effect_ = effect;
		}

		GLuint PostProcessingManager::ApplyEffects(
			GLuint           sourceTexture,
			GLuint           depthTexture,
			GLuint           normalTexture,
			GLuint           pbrTexture,
			const glm::mat4& viewMatrix,
			const glm::mat4& projectionMatrix,
			const glm::mat4& prevViewMatrix,
			const glm::mat4& prevProjectionMatrix,
			const glm::vec3& cameraPos,
			float            time,
			uint32_t         frameCount
		) {
			bool   effect_applied = false;
			int    fbo_index = 0;
			GLuint current_texture = sourceTexture;

			// Ensure the viewport is set correctly for our FBOs before starting
			glViewport(0, 0, width_, height_);

			// Generate Hi-Z depth mipmap chain
			GenerateHiZ(depthTexture);

			PostProcessingParams params;
			params.depthTexture = depthTexture;
			params.hizTexture = hiz_texture_;
			params.normalTexture = normalTexture;
			params.pbrTexture = pbrTexture;
			params.viewMatrix = viewMatrix;
			params.projectionMatrix = projectionMatrix;
			params.invViewMatrix = glm::inverse(viewMatrix);
			params.invProjectionMatrix = glm::inverse(projectionMatrix);
			params.prevViewMatrix = prevViewMatrix;
			params.prevProjectionMatrix = prevProjectionMatrix;
			params.cameraPos = cameraPos;
			params.time = time;
			params.frameCount = frameCount;
			params.motionVectorTexture = motion_vector_texture_;

			// 1. Calculate Motion Vectors first (if enabled)
			if (motion_vector_effect_ && motion_vector_effect_->IsEnabled()) {
				motion_vector_effect_->SetTime(time);
				glBindFramebuffer(GL_FRAMEBUFFER, motion_vector_fbo_);
				glClear(GL_COLOR_BUFFER_BIT);
				params.sourceTexture = current_texture;
				glBindVertexArray(quad_vao_);
				motion_vector_effect_->Apply(params);
				glBindVertexArray(0);
			}

			// Pre-tone-mapping effects chain
			for (const auto& effect : pre_tone_mapping_effects_) {
				if (effect->IsEnabled()) {
					effect->SetTime(time);
					glBindFramebuffer(GL_FRAMEBUFFER, pingpong_fbo_[fbo_index]);
					glClear(GL_COLOR_BUFFER_BIT);

					params.sourceTexture = current_texture;

					glBindVertexArray(quad_vao_);
					effect->Apply(params);
					glBindVertexArray(0);

					current_texture = pingpong_texture_[fbo_index];
					fbo_index = 1 - fbo_index; // Flip index
					effect_applied = true;
				}
			}

			// Apply the tone mapping effect as the final step
			if (tone_mapping_effect_ && tone_mapping_effect_->IsEnabled()) {
				tone_mapping_effect_->SetTime(time);
				glBindFramebuffer(GL_FRAMEBUFFER, pingpong_fbo_[fbo_index]);
				glClear(GL_COLOR_BUFFER_BIT);

				params.sourceTexture = current_texture;

				glBindVertexArray(quad_vao_);
				tone_mapping_effect_->Apply(params);
				glBindVertexArray(0);

				current_texture = pingpong_texture_[fbo_index];
				effect_applied = true;
			}

			// Restore the default framebuffer
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			if (!effect_applied) {
				return sourceTexture;
			}

			return current_texture;
		}

		void PostProcessingManager::GenerateHiZ(GLuint depthTexture) {
			if (!hiz_shader_ || !hiz_shader_->isValid() || !hiz_texture_)
				return;

			int levels = (int)std::floor(std::log2(std::max(width_, height_))) + 1;
			int currW = width_;
			int currH = height_;

			hiz_shader_->use();

			// Pass 0: Copy depthTexture to hiz_texture_ Level 0
			hiz_shader_->setInt("u_Mode", 0);
			hiz_shader_->setInt("u_SrcLevel", 0);
			hiz_shader_->setVec2("u_DestSize", (float)currW, (float)currH);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, depthTexture);
			glBindImageTexture(1, hiz_texture_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
			hiz_shader_->dispatch((currW + 7) / 8, (currH + 7) / 8, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			// Pass 1..N: Downsample
			hiz_shader_->setInt("u_Mode", 1);
			glBindTexture(GL_TEXTURE_2D, hiz_texture_);
			for (int i = 1; i < levels; ++i) {
				currW = std::max(1, currW / 2);
				currH = std::max(1, currH / 2);

				hiz_shader_->setInt("u_SrcLevel", i - 1);
				hiz_shader_->setVec2("u_DestSize", (float)currW, (float)currH);
				glBindImageTexture(1, hiz_texture_, i, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

				hiz_shader_->dispatch((currW + 7) / 8, (currH + 7) / 8, 1);
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
			}
		}

		void PostProcessingManager::Resize(int width, int height) {
			width_ = width;
			height_ = height;
			InitializeFBOs();

			if (motion_vector_effect_) {
				motion_vector_effect_->Resize(width, height);
			}

			for (const auto& effect : pre_tone_mapping_effects_) {
				effect->Resize(width, height);
			}

			if (tone_mapping_effect_) {
				tone_mapping_effect_->Resize(width, height);
			}
		}

	} // namespace PostProcessing
} // namespace Boidsish
