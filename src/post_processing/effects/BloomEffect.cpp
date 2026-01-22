#include "post_processing/effects/BloomEffect.h"

#include "logger.h"
#include "shader.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Boidsish {
	namespace PostProcessing {

		BloomEffect::BloomEffect(int width, int height):
			_width(width), _height(height), _brightPassFBO(0), _brightPassTexture(0) {
			name_ = "Bloom";
		}

		BloomEffect::~BloomEffect() {
			glDeleteFramebuffers(1, &_brightPassFBO);
			glDeleteTextures(1, &_brightPassTexture);
			for (const auto& mip : _mipChain) {
				glDeleteFramebuffers(1, &mip.fbo);
				glDeleteTextures(1, &mip.texture);
			}
		}

		void BloomEffect::Initialize(int width, int height) {
			_width = width;
			_height = height;

			_brightPassShader = std::make_unique<Shader>(
				"shaders/postprocess.vert",
				"shaders/effects/bright_pass.frag"
			);
			_downsampleShader = std::make_unique<Shader>(
				"shaders/postprocess.vert",
				"shaders/effects/bloom_downsample.frag"
			);
			_upsampleShader = std::make_unique<Shader>(
				"shaders/postprocess.vert",
				"shaders/effects/bloom_upsample.frag"
			);
			_compositeShader = std::make_unique<Shader>(
				"shaders/postprocess.vert",
				"shaders/effects/bloom_composite.frag"
			);

			InitializeFBOs();
		}

		void BloomEffect::InitializeFBOs() {
			glDeleteFramebuffers(1, &_brightPassFBO);
			glDeleteTextures(1, &_brightPassTexture);
			for (const auto& mip : _mipChain) {
				glDeleteFramebuffers(1, &mip.fbo);
				glDeleteTextures(1, &mip.texture);
			}
			_mipChain.clear();

			// Bright pass FBO
			glGenFramebuffers(1, &_brightPassFBO);
			glBindFramebuffer(GL_FRAMEBUFFER, _brightPassFBO);
			glGenTextures(1, &_brightPassTexture);
			glBindTexture(GL_TEXTURE_2D, _brightPassTexture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, _width, _height, 0, GL_RGB, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _brightPassTexture, 0);
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				logger::ERROR("Bloom Bright Pass FBO is not complete!");

			glm::vec2 mipSize((float)_width, (float)_height);

			for (int i = 0; i < 5; i++) {
				BloomMip mip;
				mipSize /= 2.0f;
				mip.size = mipSize;

				glGenFramebuffers(1, &mip.fbo);
				glBindFramebuffer(GL_FRAMEBUFFER, mip.fbo);

				glGenTextures(1, &mip.texture);
				glBindTexture(GL_TEXTURE_2D, mip.texture);
				glTexImage2D(
					GL_TEXTURE_2D,
					0,
					GL_RGB16F,
					(int)mip.size.x,
					(int)mip.size.y,
					0,
					GL_RGB,
					GL_FLOAT,
					nullptr
				);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mip.texture, 0);

				if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
					logger::ERROR("Bloom mip FBO " + std::to_string(i) + " is not complete!");
				}
				_mipChain.push_back(mip);
			}

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		void BloomEffect::Apply(GLuint sourceTexture) {
			GLint originalFBO;
			glGetIntegerv(GL_FRAMEBUFFER_BINDING, &originalFBO);
			GLint originalViewport[4];
			glGetIntegerv(GL_VIEWPORT, originalViewport);

			// 1. Bright pass - extract bright pixels
			glBindFramebuffer(GL_FRAMEBUFFER, _brightPassFBO);
			glViewport(0, 0, _width, _height);
			_brightPassShader->use();
			_brightPassShader->setInt("sceneTexture", 0);
			_brightPassShader->setFloat("threshold", threshold_);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glDrawArrays(GL_TRIANGLES, 0, 6);

			// 2. Progressive downsample with blur
			_downsampleShader->use();
			GLuint    currentTexture = _brightPassTexture;
			glm::vec2 currentSize(_width, _height);

			for (size_t i = 0; i < _mipChain.size(); i++) {
				const auto& mip = _mipChain[i];

				glBindFramebuffer(GL_FRAMEBUFFER, mip.fbo);
				glViewport(0, 0, mip.size.x, mip.size.y);

				_downsampleShader->setVec2("srcResolution", currentSize.x, currentSize.y);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, currentTexture);
				glDrawArrays(GL_TRIANGLES, 0, 6);

				currentTexture = mip.texture;
				currentSize = mip.size;
			}

			// 3. Progressive upsample and accumulate
			_upsampleShader->use();
			_upsampleShader->setFloat("filterRadius", 1.0f);

			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE);
			glBlendEquation(GL_FUNC_ADD);

			for (int i = (int)_mipChain.size() - 1; i > 0; i--) {
				const auto& srcMip = _mipChain[i];
				const auto& dstMip = _mipChain[i - 1];

				glBindFramebuffer(GL_FRAMEBUFFER, dstMip.fbo);
				glViewport(0, 0, dstMip.size.x, dstMip.size.y);

				_upsampleShader->setVec2("srcResolution", srcMip.size.x, srcMip.size.y);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, srcMip.texture);
				glDrawArrays(GL_TRIANGLES, 0, 6);
			}

			glDisable(GL_BLEND);

			// 4. Final composite with scene
			glBindFramebuffer(GL_FRAMEBUFFER, originalFBO);
			glViewport(originalViewport[0], originalViewport[1], originalViewport[2], originalViewport[3]);
			_compositeShader->use();
			_compositeShader->setInt("sceneTexture", 0);
			_compositeShader->setInt("bloomBlur", 1);
			_compositeShader->setFloat("intensity", intensity_);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, _mipChain[0].texture);
			glDrawArrays(GL_TRIANGLES, 0, 6);

			// Cleanup
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, 0);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		void BloomEffect::Resize(int width, int height) {
			_width = width;
			_height = height;
			InitializeFBOs();
		}

	} // namespace PostProcessing
} // namespace Boidsish
