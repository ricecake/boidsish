#include "post_processing/effects/SsaoEffect.h"

#include <random>

#include "shader.h"
#include <GL/glew.h>

namespace Boidsish {
	namespace PostProcessing {

		SsaoEffect::SsaoEffect() {
			name_ = "SSAO";
		}

		SsaoEffect::~SsaoEffect() {
			if (ssao_fbo_) glDeleteFramebuffers(1, &ssao_fbo_);
			if (ssao_texture_) glDeleteTextures(1, &ssao_texture_);
			if (noise_texture_) glDeleteTextures(1, &noise_texture_);
			if (blur_fbo_) glDeleteFramebuffers(1, &blur_fbo_);
			if (blur_texture_) glDeleteTextures(1, &blur_texture_);
		}

		void SsaoEffect::Initialize(int width, int height) {
			width_ = width;
			height_ = height;

			ssao_shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/ssao.frag");
			blur_shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/ssao_blur.frag");
			composite_shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/ssao_composite.frag");

			InitializeFBOs();
			GenerateKernel();
			GenerateNoiseTexture();
		}

		void SsaoEffect::InitializeFBOs() {
			// SSAO FBO
			glGenFramebuffers(1, &ssao_fbo_);
			glBindFramebuffer(GL_FRAMEBUFFER, ssao_fbo_);
			glGenTextures(1, &ssao_texture_);
			glBindTexture(GL_TEXTURE_2D, ssao_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width_, height_, 0, GL_RED, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssao_texture_, 0);

			// Blur FBO
			glGenFramebuffers(1, &blur_fbo_);
			glBindFramebuffer(GL_FRAMEBUFFER, blur_fbo_);
			glGenTextures(1, &blur_texture_);
			glBindTexture(GL_TEXTURE_2D, blur_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width_, height_, 0, GL_RED, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, blur_texture_, 0);

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		void SsaoEffect::GenerateKernel() {
			std::uniform_real_distribution<float> random_floats(0.0, 1.0);
			std::default_random_engine             generator;
			ssao_kernel_.clear();
			for (unsigned int i = 0; i < 64; ++i) {
				glm::vec3 sample(random_floats(generator) * 2.0 - 1.0, random_floats(generator) * 2.0 - 1.0, random_floats(generator));
				sample = glm::normalize(sample);
				sample *= random_floats(generator);
				float scale = float(i) / 64.0;

				scale = glm::mix(0.1f, 1.0f, scale * scale);
				sample *= scale;
				ssao_kernel_.push_back(sample);
			}
		}

		void SsaoEffect::GenerateNoiseTexture() {
			std::uniform_real_distribution<float> random_floats(0.0, 1.0);
			std::default_random_engine             generator;
			std::vector<glm::vec3>                 ssao_noise;
			for (unsigned int i = 0; i < 16; i++) {
				glm::vec3 noise(random_floats(generator) * 2.0 - 1.0, random_floats(generator) * 2.0 - 1.0, 0.0f);
				ssao_noise.push_back(noise);
			}
			glGenTextures(1, &noise_texture_);
			glBindTexture(GL_TEXTURE_2D, noise_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, 4, 4, 0, GL_RGB, GL_FLOAT, &ssao_noise[0]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		}

		void SsaoEffect::Apply(const PostProcessingParams& params) {
			GLint originalFBO;
			glGetIntegerv(GL_FRAMEBUFFER_BINDING, &originalFBO);

			// 1. SSAO PASS
			glBindFramebuffer(GL_FRAMEBUFFER, ssao_fbo_);
			glViewport(0, 0, width_, height_);
			glClear(GL_COLOR_BUFFER_BIT);
			ssao_shader_->use();
			for (unsigned int i = 0; i < 64; ++i)
				ssao_shader_->setVec3("samples[" + std::to_string(i) + "]", ssao_kernel_[i]);
			ssao_shader_->setMat4("projection", params.projectionMatrix);
			ssao_shader_->setMat4("view", params.viewMatrix);
			ssao_shader_->setFloat("radius", radius_);
			ssao_shader_->setFloat("bias", bias_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, params.normalTexture);
			ssao_shader_->setInt("gNormal", 0);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, params.depthTexture);
			ssao_shader_->setInt("gDepth", 1);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, noise_texture_);
			ssao_shader_->setInt("texNoise", 2);
			glDrawArrays(GL_TRIANGLES, 0, 6);

			// 2. BLUR
			glBindFramebuffer(GL_FRAMEBUFFER, blur_fbo_);
			blur_shader_->use();
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, ssao_texture_);
			blur_shader_->setInt("ssaoInput", 0);
			glDrawArrays(GL_TRIANGLES, 0, 6);

			// 3. COMPOSITE
			glBindFramebuffer(GL_FRAMEBUFFER, originalFBO);
			glViewport(0, 0, width_, height_);
			composite_shader_->use();
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, params.sourceTexture);
			composite_shader_->setInt("sceneTexture", 0);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, blur_texture_);
			composite_shader_->setInt("ssaoTexture", 1);
			composite_shader_->setFloat("intensity", intensity_);
			composite_shader_->setFloat("power", power_);
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void SsaoEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
			if (ssao_texture_) glDeleteTextures(1, &ssao_texture_);
			if (ssao_fbo_) glDeleteFramebuffers(1, &ssao_fbo_);
			if (blur_texture_) glDeleteTextures(1, &blur_texture_);
			if (blur_fbo_) glDeleteFramebuffers(1, &blur_fbo_);
			InitializeFBOs();
		}

	} // namespace PostProcessing
} // namespace Boidsish
