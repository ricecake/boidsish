#include "post_processing/effects/BloomEffect.h"

#include "logger.h"
#include "shader.h"
#include "constants.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Boidsish {
	namespace PostProcessing {

		BloomEffect::BloomEffect(int width, int height):
			_width(width), _height(height), _bloomTexture(0) {
			name_ = "Bloom";
		}

		BloomEffect::~BloomEffect() {
			if (_bloomTexture) glDeleteTextures(1, &_bloomTexture);
			if (!_upsampleFBOs.empty()) {
				glDeleteFramebuffers((GLsizei)_upsampleFBOs.size(), _upsampleFBOs.data());
			}
			if (_exposureSsbo) {
				glDeleteBuffers(1, &_exposureSsbo);
			}
		}

		void BloomEffect::Initialize(int width, int height) {
			_width = width;
			_height = height;

			_downsampleComputeShader = std::make_unique<ComputeShader>(
				"shaders/effects/bloom_downsample.comp"
			);
			_upsampleShader = std::make_unique<Shader>(
				"shaders/postprocess.vert",
				"shaders/effects/bloom_upsample.frag"
			);
			_compositeShader = std::make_unique<Shader>(
				"shaders/postprocess.vert",
				"shaders/effects/bloom_composite.frag"
			);

			InitializeResources();
		}

		void BloomEffect::InitializeResources() {
			if (_bloomTexture) glDeleteTextures(1, &_bloomTexture);
			if (!_upsampleFBOs.empty()) {
				glDeleteFramebuffers((GLsizei)_upsampleFBOs.size(), _upsampleFBOs.data());
				_upsampleFBOs.clear();
			}

			// Mipmapped Bloom Texture
			_numMips = 5;
			glGenTextures(1, &_bloomTexture);
			glBindTexture(GL_TEXTURE_2D, _bloomTexture);
			glTexStorage2D(GL_TEXTURE_2D, _numMips, GL_RGBA16F, _width / 2, _height / 2);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			// Create FBOs for upsampling into mip levels
			_upsampleFBOs.resize(_numMips);
			glGenFramebuffers(_numMips, _upsampleFBOs.data());
			for (int i = 0; i < _numMips; i++) {
				glBindFramebuffer(GL_FRAMEBUFFER, _upsampleFBOs[i]);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _bloomTexture, i);
				if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
					logger::ERROR("Bloom upsample FBO " + std::to_string(i) + " is not complete!");
				}
			}

			// Auto-exposure SSBO
			if (_exposureSsbo == 0) {
				struct ExposureData {
					float adaptedLuminance;
					float targetLuminance;
					float minExposure;
					float maxExposure;
					int   useAutoExposure;
					uint32_t totalLogLuma;
					uint32_t totalPixelCount;
					uint32_t workgroupCounter;
				};

				glGenBuffers(1, &_exposureSsbo);
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, _exposureSsbo);
				ExposureData initialData = {0.3f, _targetLuminance, _minExposure, _maxExposure, 1, 0, 0, 0};
				glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(ExposureData), &initialData, GL_DYNAMIC_DRAW);
				glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::AutoExposure(), _exposureSsbo);
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
			}

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		void BloomEffect::Apply(GLuint sourceTexture, GLuint depthTexture, GLuint velocityTexture, GLuint normalTexture, GLuint albedoTexture, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraPos) {
			GLint originalFBO;
			glGetIntegerv(GL_FRAMEBUFFER_BINDING, &originalFBO);
			GLint originalViewport[4];
			glGetIntegerv(GL_VIEWPORT, originalViewport);

			// 1. Update Auto-Exposure SSBO parameters
			struct ExposureData {
				float adaptedLuminance;
				float targetLuminance;
				float minExposure;
				float maxExposure;
				int   useAutoExposure;
			};
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, _exposureSsbo);
			float actualTarget = _targetLuminance * (1.0f - _nightFactor * 0.5f);
			float actualMax = _maxExposure * (1.0f - _nightFactor * 0.4f);
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, offsetof(ExposureData, targetLuminance), sizeof(float), &actualTarget);
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, offsetof(ExposureData, minExposure), sizeof(float), &_minExposure);
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, offsetof(ExposureData, maxExposure), sizeof(float), &actualMax);
			int enabled = _autoExposureEnabled ? 1 : 0;
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, offsetof(ExposureData, useAutoExposure), sizeof(int), &enabled);

			// 2. Compute-based Downsample, Bright Pass and Auto-Exposure
			_downsampleComputeShader->use();
			_downsampleComputeShader->setVec2("srcResolution", (float)_width, (float)_height);
			_downsampleComputeShader->setInt("numMips", _numMips);
			_downsampleComputeShader->setFloat("threshold", threshold_);
			_downsampleComputeShader->setFloat("deltaTime", _deltaTime);
			_downsampleComputeShader->setFloat("speedUp", _speedUp);
			_downsampleComputeShader->setFloat("speedDown", _speedDown);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);

			for (int i = 0; i < _numMips; i++) {
				glBindImageTexture(i, _bloomTexture, i, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
			}
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::AutoExposure(), _exposureSsbo);

			unsigned int groupsX = (_width / 2 + 15) / 16;
			unsigned int groupsY = (_height / 2 + 15) / 16;
			_downsampleComputeShader->dispatch(groupsX, groupsY, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

			// 3. Progressive upsample and accumulate
			_upsampleShader->use();
			_upsampleShader->setFloat("filterRadius", 1.0f);

			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE);
			glBlendEquation(GL_FUNC_ADD);

			for (int i = _numMips - 1; i > 0; i--) {
				int srcMip = i;
				int dstMip = i - 1;

				int dstWidth = (_width / 2) >> dstMip;
				int dstHeight = (_height / 2) >> dstMip;
				int srcWidth = (_width / 2) >> srcMip;
				int srcHeight = (_height / 2) >> srcMip;

				glBindFramebuffer(GL_FRAMEBUFFER, _upsampleFBOs[dstMip]);
				glViewport(0, 0, dstWidth, dstHeight);

				_upsampleShader->setVec2("srcResolution", (float)srcWidth, (float)srcHeight);

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, _bloomTexture);
				_upsampleShader->setFloat("srcLod", (float)srcMip);

				glDrawArrays(GL_TRIANGLES, 0, 6);
			}

			glDisable(GL_BLEND);

			// 4. Final composite with scene and integrated tonemapping
			glBindFramebuffer(GL_FRAMEBUFFER, originalFBO);
			glViewport(originalViewport[0], originalViewport[1], originalViewport[2], originalViewport[3]);
			_compositeShader->use();
			_compositeShader->setInt("sceneTexture", 0);
			_compositeShader->setInt("bloomBlur", 1);
			_compositeShader->setFloat("intensity", intensity_);
			_compositeShader->setFloat("minIntensity", minIntensity_);
			_compositeShader->setFloat("maxIntensity", maxIntensity_);

			_compositeShader->setBool("toneMappingEnabled", _toneMappingEnabled);
			_compositeShader->setInt("toneMapMode", _toneMappingMode);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, _bloomTexture);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::AutoExposure(), _exposureSsbo);

			glDrawArrays(GL_TRIANGLES, 0, 6);

			// Reset mip levels for future use
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, _numMips - 1);

			// Cleanup
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, 0);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		void BloomEffect::Resize(int width, int height) {
			_width = width;
			_height = height;
			InitializeResources();
		}

		void BloomEffect::SetTime(float time) {
			if (_lastTime > 0.0f) {
				_deltaTime = time - _lastTime;
			}
			_lastTime = time;
		}

	} // namespace PostProcessing
} // namespace Boidsish
