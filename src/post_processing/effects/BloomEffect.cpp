#include "post_processing/effects/BloomEffect.h"

#include "logger.h"
#include "shader.h"
#include "constants.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Boidsish {
	namespace PostProcessing {

		BloomEffect::BloomEffect(int width, int height):
			_width(width), _height(height), _bloomTexture(0), _ltmExpTexture(0), _ltmWgtTexture(0), _ltmFusedTexture(0) {
			name_ = "Bloom";
			_sky.targetLuminance = 0.5f; // Sky usually wants to be a bit brighter
			_sky.ltmEnabled = false; // Usually don't need LTM on sky
		}

		BloomEffect::~BloomEffect() {
			if (_bloomTexture) glDeleteTextures(1, &_bloomTexture);
			if (_ltmExpTexture) glDeleteTextures(1, &_ltmExpTexture);
			if (_ltmWgtTexture) glDeleteTextures(1, &_ltmWgtTexture);
			if (_ltmFusedTexture) glDeleteTextures(1, &_ltmFusedTexture);
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
			_ltmFuseComputeShader = std::make_unique<ComputeShader>(
				"shaders/effects/ltm_fuse.comp"
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
			if (_ltmExpTexture) glDeleteTextures(1, &_ltmExpTexture);
			if (_ltmWgtTexture) glDeleteTextures(1, &_ltmWgtTexture);
			if (_ltmFusedTexture) glDeleteTextures(1, &_ltmFusedTexture);

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

			// Mipmapped LTM Texture (3 exposures)
			glGenTextures(1, &_ltmExpTexture);
			glBindTexture(GL_TEXTURE_2D, _ltmExpTexture);
			glTexStorage2D(GL_TEXTURE_2D, _numMips, GL_RGBA16F, _width / 2, _height / 2);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			// Mipmapped LTM Weights Texture (3 weights)
			glGenTextures(1, &_ltmWgtTexture);
			glBindTexture(GL_TEXTURE_2D, _ltmWgtTexture);
			glTexStorage2D(GL_TEXTURE_2D, _numMips, GL_RGBA16F, _width / 2, _height / 2);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			// Fused LTM Result (Quarter-res target for Guided Upsampling)
			glGenTextures(1, &_ltmFusedTexture);
			glBindTexture(GL_TEXTURE_2D, _ltmFusedTexture);
			glTexStorage2D(GL_TEXTURE_2D, 1, GL_R16F, _width / 2, _height / 2);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
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
				glGenBuffers(1, &_exposureSsbo);
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, _exposureSsbo);

				ExposureData initialData = {};
				initialData.workgroupCounter = 0;

				auto setupLayer = [&](LayerData& data, const LayerSettings& settings) {
					data.adaptedLuminance = 0.3f;
					data.targetLuminance = settings.targetLuminance;
					data.minExposure = settings.minExposure;
					data.maxExposure = settings.maxExposure;
					data.useAutoExposure = settings.autoExposureEnabled ? 1 : 0;
					data.centerWeightTightness = settings.centerWeightTightness;
					data.focusPoint = settings.focusPoint;
					data.histogramLowCutoff = settings.histogramLowCutoff;
					data.histogramHighCutoff = settings.histogramHighCutoff;
					data.speedUp = settings.speedUp;
					data.speedDown = settings.speedDown;

					data.autoTuneEnabled = settings.autoTuneEnabled ? 1 : 0;
					data.minContrast = settings.minContrast;
					data.maxContrast = settings.maxContrast;
					data.targetBrightness = settings.targetBrightness;

					data.cdlSlope = glm::vec4(settings.cdlSlope, 0.0f);
					data.cdlOffset = glm::vec4(settings.cdlOffset, 0.0f);
					data.cdlPower = glm::vec4(settings.cdlPower, 0.0f);
					data.cdlSaturation = settings.cdlSaturation;

					data.whiteTemp = settings.whiteTemp;
					data.whiteTint = settings.whiteTint;

					data.ltmEnabled = settings.ltmEnabled ? 1 : 0;
					data.ltmEvSpread = settings.ltmEvSpread;
					data.ltmTarget = settings.ltmTarget;
					data.ltmSigma = settings.ltmSigma;
					data.ltmWeightContrast = settings.ltmWeightContrast;
					data.ltmWeightSaturation = settings.ltmWeightSaturation;
					data.ltmWeightExposedness = settings.ltmWeightExposedness;
					data.ltmBoostLocalContrast = settings.ltmBoostLocalContrast;

					for (int i = 0; i < 256; i++) data.histogram[i] = 0;
				};

				setupLayer(initialData.scene, _scene);
				setupLayer(initialData.sky, _sky);

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
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, _exposureSsbo);

			auto updateLayer = [&](const LayerSettings& settings, size_t offset) {
				float actualTarget = settings.targetLuminance * (1.0f - _nightFactor * 0.5f);
				float actualMax = settings.maxExposure * (1.0f - _nightFactor * 0.4f);

				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, targetLuminance), sizeof(float), &actualTarget);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, minExposure), sizeof(float), &settings.minExposure);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, maxExposure), sizeof(float), &actualMax);

				int enabled = settings.autoExposureEnabled ? 1 : 0;
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, useAutoExposure), sizeof(int), &enabled);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, centerWeightTightness), sizeof(float), &settings.centerWeightTightness);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, focusPoint), sizeof(glm::vec2), &settings.focusPoint);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, histogramLowCutoff), sizeof(float), &settings.histogramLowCutoff);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, histogramHighCutoff), sizeof(float), &settings.histogramHighCutoff);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, speedUp), sizeof(float), &settings.speedUp);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, speedDown), sizeof(float), &settings.speedDown);

				int autoTune = settings.autoTuneEnabled ? 1 : 0;
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, autoTuneEnabled), sizeof(int), &autoTune);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, minContrast), sizeof(float), &settings.minContrast);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, maxContrast), sizeof(float), &settings.maxContrast);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, targetBrightness), sizeof(float), &settings.targetBrightness);

				glm::vec4 slope4(settings.cdlSlope, 0.0f);
				glm::vec4 offset4(settings.cdlOffset, 0.0f);
				glm::vec4 power4(settings.cdlPower, 0.0f);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, cdlSlope), sizeof(glm::vec4), &slope4);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, cdlOffset), sizeof(glm::vec4), &offset4);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, cdlPower), sizeof(glm::vec4), &power4);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, cdlSaturation), sizeof(float), &settings.cdlSaturation);

				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, whiteTemp), sizeof(float), &settings.whiteTemp);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, whiteTint), sizeof(float), &settings.whiteTint);

				int ltmEnabled = settings.ltmEnabled ? 1 : 0;
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, ltmEnabled), sizeof(int), &ltmEnabled);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, ltmEvSpread), sizeof(float), &settings.ltmEvSpread);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, ltmTarget), sizeof(float), &settings.ltmTarget);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, ltmSigma), sizeof(float), &settings.ltmSigma);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, ltmWeightContrast), sizeof(float), &settings.ltmWeightContrast);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, ltmWeightSaturation), sizeof(float), &settings.ltmWeightSaturation);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, ltmWeightExposedness), sizeof(float), &settings.ltmWeightExposedness);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, ltmBoostLocalContrast), sizeof(float), &settings.ltmBoostLocalContrast);
			};

			updateLayer(_scene, offsetof(ExposureData, scene));
			updateLayer(_sky, offsetof(ExposureData, sky));

			// 2. Compute-based Downsample, Bright Pass and Auto-Exposure
			_downsampleComputeShader->use();
			_downsampleComputeShader->setVec2("srcResolution", (float)_width, (float)_height);
			_downsampleComputeShader->setInt("numMips", _numMips);
			_downsampleComputeShader->setFloat("threshold", threshold_);
			_downsampleComputeShader->setFloat("deltaTime", _deltaTime);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, depthTexture);

			for (int i = 0; i < _numMips; i++) {
				glBindImageTexture(5 + i, _bloomTexture, i, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
			}
			for (int i = 0; i < _numMips; i++) {
				glBindImageTexture(10 + i, _ltmExpTexture, i, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
			}
			for (int i = 0; i < _numMips; i++) {
				glBindImageTexture(15 + i, _ltmWgtTexture, i, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
			}
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::AutoExposure(), _exposureSsbo);

			unsigned int groupsX = (_width / 2 + 15) / 16;
			unsigned int groupsY = (_height / 2 + 15) / 16;
			_downsampleComputeShader->dispatch(groupsX, groupsY, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

			// Unbind image units
			for (int i = 5; i < 20; i++) {
				glBindImageTexture(i, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
			}

			// 2.5 LTM Fusion
			if (_scene.ltmEnabled || _sky.ltmEnabled) {
				_ltmFuseComputeShader->use();
				_ltmFuseComputeShader->setInt("expTexture", 0);
				_ltmFuseComputeShader->setInt("wgtTexture", 1);
				_ltmFuseComputeShader->setInt("depthTexture", 3);
				_ltmFuseComputeShader->setInt("startMip", _numMips - 1);
				_ltmFuseComputeShader->setInt("endMip", 0);

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, _ltmExpTexture);
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, _ltmWgtTexture);
				glActiveTexture(GL_TEXTURE3);
				glBindTexture(GL_TEXTURE_2D, depthTexture);

				glBindImageTexture(2, _ltmFusedTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R16F);

				glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::AutoExposure(), _exposureSsbo);

				_ltmFuseComputeShader->dispatch(groupsX, groupsY, 1);
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
				glBindImageTexture(2, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R16F);
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
			_compositeShader->setInt("ltmFused", 2);
			_compositeShader->setInt("ltmExpMip", 3);
			_compositeShader->setInt("depthTexture", 4);
			_compositeShader->setVec2("ltmRes", (float)(_width / 2), (float)(_height / 2));
			_compositeShader->setFloat("intensity", intensity_);
			_compositeShader->setFloat("minIntensity", minIntensity_);
			_compositeShader->setFloat("maxIntensity", maxIntensity_);

			_compositeShader->setFloat("farPlane", Constants::Project::Camera::DefaultFarPlane());
			_compositeShader->setFloat("nearPlane", Constants::Project::Camera::DefaultNearPlane());

			_compositeShader->setBool("sceneToneMappingEnabled", _scene.toneMappingEnabled);
			_compositeShader->setInt("sceneToneMapMode", _scene.toneMappingMode);
			_compositeShader->setBool("skyToneMappingEnabled", _sky.toneMappingEnabled);
			_compositeShader->setInt("skyToneMapMode", _sky.toneMappingMode);

			// Manual Uchimura parameters for scene
			_compositeShader->setFloat("sceneUchimuraP", _scene.uchimuraP);
			_compositeShader->setFloat("sceneUchimuraA", _scene.uchimuraA);
			_compositeShader->setFloat("sceneUchimuraM", _scene.uchimuraM);
			_compositeShader->setFloat("sceneUchimuraL", _scene.uchimuraL);
			_compositeShader->setFloat("sceneUchimuraC", _scene.uchimuraC);
			_compositeShader->setFloat("sceneUchimuraB", _scene.uchimuraB);

			// Manual Uchimura parameters for sky
			_compositeShader->setFloat("skyUchimuraP", _sky.uchimuraP);
			_compositeShader->setFloat("skyUchimuraA", _sky.uchimuraA);
			_compositeShader->setFloat("skyUchimuraM", _sky.uchimuraM);
			_compositeShader->setFloat("skyUchimuraL", _sky.uchimuraL);
			_compositeShader->setFloat("skyUchimuraC", _sky.uchimuraC);
			_compositeShader->setFloat("skyUchimuraB", _sky.uchimuraB);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, _bloomTexture);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, _ltmFusedTexture);

			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, _ltmExpTexture);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

			glActiveTexture(GL_TEXTURE4);
			glBindTexture(GL_TEXTURE_2D, depthTexture);

			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::AutoExposure(), _exposureSsbo);

			glDrawArrays(GL_TRIANGLES, 0, 6);

			// Reset mip levels for future use
			glActiveTexture(GL_TEXTURE1);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, _numMips - 1);

			glActiveTexture(GL_TEXTURE3);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, _numMips - 1);

			// Cleanup
			glActiveTexture(GL_TEXTURE4);
			glBindTexture(GL_TEXTURE_2D, 0);
			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, 0);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, 0);
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
