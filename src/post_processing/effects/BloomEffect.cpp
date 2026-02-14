#include "post_processing/effects/BloomEffect.h"

#include <iostream>

#include "shader.h"
#include <GL/glew.h>

namespace Boidsish {
	namespace PostProcessing {

		BloomEffect::BloomEffect(int width, int height): _width(width), _height(height) {
			name_ = "Bloom";
		}

		BloomEffect::~BloomEffect() {
			for (auto& mip : _mipChain) {
				glDeleteFramebuffers(1, &mip.fbo);
				glDeleteTextures(1, &mip.texture);
			}
			glDeleteFramebuffers(1, &_brightPassFBO);
			glDeleteTextures(1, &_brightPassTexture);
		}

		void BloomEffect::Initialize(int width, int height) {
			_width = width;
			_height = height;

			// Load shaders
			_downsampleShader = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/bloom_downsample.frag");
			_upsampleShader = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/bloom_upsample.frag");
			_compositeShader = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/bloom_composite.frag");
			_brightPassShader = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/bright_pass.frag");

			InitializeFBOs();
		}

		void BloomEffect::InitializeFBOs() {
			// Bright Pass FBO
			glGenFramebuffers(1, &_brightPassFBO);
			glBindFramebuffer(GL_FRAMEBUFFER, _brightPassFBO);
			glGenTextures(1, &_brightPassTexture);
			glBindTexture(GL_TEXTURE_2D, _brightPassTexture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, _width / 2, _height / 2, 0, GL_RGB, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _brightPassTexture, 0);

			// Mip Chain
			_mipChain.clear();
			int mWidth = _width / 2;
			int mHeight = _height / 2;

			for (int i = 0; i < 5; i++) { // 5 mips
				mWidth /= 2;
				mHeight /= 2;
				if (mWidth < 1) mWidth = 1;
				if (mHeight < 1) mHeight = 1;

				BloomMip mip;
				mip.size = glm::vec2(mWidth, mHeight);
				glGenFramebuffers(1, &mip.fbo);
				glBindFramebuffer(GL_FRAMEBUFFER, mip.fbo);
				glGenTextures(1, &mip.texture);
				glBindTexture(GL_TEXTURE_2D, mip.texture);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, mWidth, mHeight, 0, GL_RGB, GL_FLOAT, NULL);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mip.texture, 0);
				_mipChain.push_back(mip);
			}
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		void BloomEffect::Apply(const PostProcessingParams& params) {
			GLint originalFBO;
			glGetIntegerv(GL_FRAMEBUFFER_BINDING, &originalFBO);

			// 1. Bright Pass
			glBindFramebuffer(GL_FRAMEBUFFER, _brightPassFBO);
			glViewport(0, 0, _width / 2, _height / 2);
			glClear(GL_COLOR_BUFFER_BIT);

			_brightPassShader->use();
			_brightPassShader->setInt("sceneTexture", 0);
			_brightPassShader->setFloat("threshold", threshold_);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, params.sourceTexture);
			glDrawArrays(GL_TRIANGLES, 0, 6);

			// 2. Downsample chain
			_downsampleShader->use();
			_downsampleShader->setInt("srcTexture", 0);
			GLuint currSrcTexture = _brightPassTexture;
			for (auto& mip : _mipChain) {
				glViewport(0, 0, (int)mip.size.x, (int)mip.size.y);
				glBindFramebuffer(GL_FRAMEBUFFER, mip.fbo);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, currSrcTexture);
				glDrawArrays(GL_TRIANGLES, 0, 6);
				currSrcTexture = mip.texture;
			}

			// 3. Upsample chain with additive blending
			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE);
			_upsampleShader->use();
			_upsampleShader->setInt("srcTexture", 0);
			_upsampleShader->setFloat("filterRadius", 0.005f);

			for (int i = (int)_mipChain.size() - 1; i > 0; i--) {
				auto& destMip = _mipChain[i-1];
				auto& srcMip = _mipChain[i];
				glViewport(0, 0, (int)destMip.size.x, (int)destMip.size.y);
				glBindFramebuffer(GL_FRAMEBUFFER, destMip.fbo);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, srcMip.texture);
				glDrawArrays(GL_TRIANGLES, 0, 6);
			}

			// Final upsample to bright pass texture
			glViewport(0, 0, _width / 2, _height / 2);
			glBindFramebuffer(GL_FRAMEBUFFER, _brightPassFBO);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, _mipChain[0].texture);
			glDrawArrays(GL_TRIANGLES, 0, 6);

			glDisable(GL_BLEND);

			// 4. Composite
			glBindFramebuffer(GL_FRAMEBUFFER, originalFBO);
			glViewport(0, 0, _width, _height);
			_compositeShader->use();
			_compositeShader->setInt("sceneTexture", 0);
			_compositeShader->setInt("bloomTexture", 1);
			_compositeShader->setFloat("intensity", intensity_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, params.sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, _brightPassTexture);
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void BloomEffect::Resize(int width, int height) {
			_width = width;
			_height = height;
			for (auto& mip : _mipChain) {
				glDeleteFramebuffers(1, &mip.fbo);
				glDeleteTextures(1, &mip.texture);
			}
			glDeleteFramebuffers(1, &_brightPassFBO);
			glDeleteTextures(1, &_brightPassTexture);
			InitializeFBOs();
		}

	} // namespace PostProcessing
} // namespace Boidsish
