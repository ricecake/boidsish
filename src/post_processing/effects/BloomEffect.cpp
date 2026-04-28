#include "post_processing/effects/BloomEffect.h"

#include <cstring>

#include "logger.h"
#include "shader.h"
#include "constants.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Boidsish {
	namespace PostProcessing {

		BloomEffect::BloomEffect(int width, int height):
			_width(width), _height(height) {
			name_ = "Bloom";
		}

		BloomEffect::~BloomEffect() {
			if (!_upsampleFBOs.empty()) {
				glDeleteFramebuffers((GLsizei)_upsampleFBOs.size(), _upsampleFBOs.data());
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
			if (!_upsampleFBOs.empty()) {
				glDeleteFramebuffers((GLsizei)_upsampleFBOs.size(), _upsampleFBOs.data());
				_upsampleFBOs.clear();
			}

			// Mipmapped Bloom Texture
			_numMips = 5;
			_bloomTexture = std::make_unique<PersistentTexture>(GL_TEXTURE_2D, GL_RGBA16F, _width / 2, _height / 2, 1, _numMips);

			GLuint bloom_id = _bloomTexture->GetId();
			glBindTexture(GL_TEXTURE_2D, bloom_id);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			// Create FBOs for upsampling into mip levels
			_upsampleFBOs.resize(_numMips);
			glGenFramebuffers(_numMips, _upsampleFBOs.data());
			for (int i = 0; i < _numMips; i++) {
				glBindFramebuffer(GL_FRAMEBUFFER, _upsampleFBOs[i]);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bloom_id, i);
				if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
					logger::ERROR("Bloom upsample FBO " + std::to_string(i) + " is not complete!");
				}
			}

			// Auto-exposure SSBO
			if (!_exposureSsbo) {
				_exposureSsbo = std::make_unique<PersistentBuffer<ExposureSsboData>>(GL_SHADER_STORAGE_BUFFER, 1, 1);
				ExposureSsboData* data = _exposureSsbo->GetFullBufferPtr();
				memset(data, 0, sizeof(ExposureSsboData));
				data->adaptedLuminance = 0.3f;
				data->targetLuminance = _targetLuminance;
				data->minExposure = _minExposure;
				data->maxExposure = _maxExposure;
				data->useAutoExposure = 1;
			}

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		void BloomEffect::Apply(GLuint sourceTexture, GLuint depthTexture, GLuint velocityTexture, GLuint normalTexture, GLuint albedoTexture, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraPos) {
			GLint originalFBO;
			glGetIntegerv(GL_FRAMEBUFFER_BINDING, &originalFBO);
			GLint originalViewport[4];
			glGetIntegerv(GL_VIEWPORT, originalViewport);

			// 1. Update Auto-Exposure SSBO parameters
			ExposureSsboData* data = _exposureSsbo->GetFullBufferPtr();
			data->targetLuminance = _targetLuminance * (1.0f - _nightFactor * 0.5f);
			data->minExposure = _minExposure;
			data->maxExposure = _maxExposure * (1.0f - _nightFactor * 0.4f);
			data->useAutoExposure = _autoExposureEnabled ? 1 : 0;

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

			GLuint bloom_id = _bloomTexture->GetId();
			for (int i = 0; i < _numMips; i++) {
				glBindImageTexture(5 + i, bloom_id, i, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
			}
			_exposureSsbo->BindBase(Constants::SsboBinding::AutoExposure());

			unsigned int groupsX = (_width / 2 + 15) / 16;
			unsigned int groupsY = (_height / 2 + 15) / 16;
			_downsampleComputeShader->dispatch(groupsX, groupsY, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

			// Unbind image units so they don't leak into later passes
			for (int i = 0; i < _numMips; i++) {
				glBindImageTexture(5 + i, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
			}

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
				glBindTexture(GL_TEXTURE_2D, bloom_id);
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
			glBindTexture(GL_TEXTURE_2D, bloom_id);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

			_exposureSsbo->BindBase(Constants::SsboBinding::AutoExposure());

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
