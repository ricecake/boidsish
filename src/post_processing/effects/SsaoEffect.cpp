#include "post_processing/effects/SsaoEffect.h"

#include <random>

#include "logger.h"
#include "shader.h"

namespace Boidsish {
	namespace PostProcessing {

		SsaoEffect::SsaoEffect() {
			name_ = "SSAO";
			is_enabled_ = false;
		}

		SsaoEffect::~SsaoEffect() {
			if (ssao_fbo_)
				glDeleteFramebuffers(1, &ssao_fbo_);
			if (ssao_texture_)
				glDeleteTextures(1, &ssao_texture_);
			if (blur_fbo_)
				glDeleteFramebuffers(1, &blur_fbo_);
			if (blur_texture_)
				glDeleteTextures(1, &blur_texture_);
			if (noise_texture_)
				glDeleteTextures(1, &noise_texture_);
		}

		void SsaoEffect::Initialize(int width, int height) {
			width_ = width;
			height_ = height;

			ssao_shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/ssao.frag");
			blur_shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/ssao_blur.frag");
			composite_shader_ = std::make_unique<Shader>(
				"shaders/postprocess.vert",
				"shaders/effects/ssao_composite.frag"
			);

			GenerateKernel();
			GenerateNoiseTexture();
			InitializeFBOs();
		}

		void SsaoEffect::InitializeFBOs() {
			if (ssao_fbo_)
				glDeleteFramebuffers(1, &ssao_fbo_);
			if (ssao_texture_)
				glDeleteTextures(1, &ssao_texture_);
			if (blur_fbo_)
				glDeleteFramebuffers(1, &blur_fbo_);
			if (blur_texture_)
				glDeleteTextures(1, &blur_texture_);

			// SSAO buffer (single channel for occlusion factor) - Use R16F for precision to avoid banding
			glGenFramebuffers(1, &ssao_fbo_);
			glBindFramebuffer(GL_FRAMEBUFFER, ssao_fbo_);
			glGenTextures(1, &ssao_texture_);
			glBindTexture(GL_TEXTURE_2D, ssao_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, width_, height_, 0, GL_RED, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssao_texture_, 0);
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				logger::ERROR("SSAO FBO is not complete!");

			// Blur buffer
			glGenFramebuffers(1, &blur_fbo_);
			glBindFramebuffer(GL_FRAMEBUFFER, blur_fbo_);
			glGenTextures(1, &blur_texture_);
			glBindTexture(GL_TEXTURE_2D, blur_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, width_, height_, 0, GL_RED, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, blur_texture_, 0);
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				logger::ERROR("SSAO Blur FBO is not complete!");

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		void SsaoEffect::GenerateKernel() {
			ssao_kernel_.clear();
			std::uniform_real_distribution<float> randomFloats(0.0, 1.0);
			std::default_random_engine            generator;
			for (unsigned int i = 0; i < 64; ++i) {
				glm::vec3 sample(
					randomFloats(generator) * 2.0 - 1.0,
					randomFloats(generator) * 2.0 - 1.0,
					randomFloats(generator)
				);
				sample = glm::normalize(sample);
				sample *= randomFloats(generator);
				float scale = (float)i / 64.0f;

				// Scale samples s.t. they're more aligned to center of kernel
				scale = 0.1f + scale * scale * (1.0f - 0.1f);
				sample *= scale;
				ssao_kernel_.push_back(sample);
			}
		}

		void SsaoEffect::GenerateNoiseTexture() {
			std::uniform_real_distribution<float> randomFloats(0.0, 1.0);
			std::default_random_engine            generator;
			std::vector<glm::vec3>                ssaoNoise;
			// Increase noise texture size to 16x16 (256 samples) to reduce tiling
			for (unsigned int i = 0; i < 256; i++) {
				glm::vec3 noise(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0, 0.0f);
				ssaoNoise.push_back(noise);
			}
			glGenTextures(1, &noise_texture_);
			glBindTexture(GL_TEXTURE_2D, noise_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, 16, 16, 0, GL_RGB, GL_FLOAT, &ssaoNoise[0]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		}

		void SsaoEffect::Apply(
			GLuint sourceTexture,
			GLuint depthTexture,
			GLuint           /* velocityTexture */,
			const glm::mat4& /* viewMatrix */,
			const glm::mat4& projectionMatrix,
			const glm::vec3& /* cameraPos */
		) {
			GLint originalFBO;
			glGetIntegerv(GL_FRAMEBUFFER_BINDING, &originalFBO);

			// 1. SSAO generation
			glBindFramebuffer(GL_FRAMEBUFFER, ssao_fbo_);
			glClear(GL_COLOR_BUFFER_BIT);
			ssao_shader_->use();
			for (unsigned int i = 0; i < 64; ++i) {
				ssao_shader_->setVec3("samples[" + std::to_string(i) + "]", ssao_kernel_[i]);
			}
			ssao_shader_->setMat4("projection", projectionMatrix);
			ssao_shader_->setMat4("invProjection", glm::inverse(projectionMatrix));
			ssao_shader_->setInt("gDepth", 0);
			ssao_shader_->setInt("texNoise", 1);
			ssao_shader_->setVec2("noiseScale", (float)width_ / 16.0f, (float)height_ / 16.0f);
			ssao_shader_->setFloat("radius", radius_);
			ssao_shader_->setFloat("bias", bias_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, depthTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, noise_texture_);
			glDrawArrays(GL_TRIANGLES, 0, 6);

			// 2. Blur SSAO - Use Bilateral Blur
			glBindFramebuffer(GL_FRAMEBUFFER, blur_fbo_);
			glClear(GL_COLOR_BUFFER_BIT);
			blur_shader_->use();
			blur_shader_->setInt("ssaoInput", 0);
			blur_shader_->setInt("gDepth", 1);
			blur_shader_->setMat4("invProjection", glm::inverse(projectionMatrix));
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, ssao_texture_);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, depthTexture);
			glDrawArrays(GL_TRIANGLES, 0, 6);

			// 3. Composite
			glBindFramebuffer(GL_FRAMEBUFFER, originalFBO);
			composite_shader_->use();
			composite_shader_->setInt("sceneTexture", 0);
			composite_shader_->setInt("ssaoTexture", 1);
			composite_shader_->setFloat("intensity", intensity_);
			composite_shader_->setFloat("power", power_);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, blur_texture_);
			glDrawArrays(GL_TRIANGLES, 0, 6);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, 0);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		void SsaoEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
			InitializeFBOs();
		}

	} // namespace PostProcessing
} // namespace Boidsish
