#include "post_processing/effects/BloomEffect.h"

#include "logger.h"
#include "state.h"
#include "service_locator.h"
#include "shader.h"
#include "constants.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Boidsish {
	namespace PostProcessing {

		BloomEffect::BloomEffect(int width, int height):
			_width(width), _height(height), _bloomTexture(0), _ltmExpTexture(0), _ltmWgtTexture(0), _ltmFusedTexture(0) {
			name_ = "Bloom";
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

				auto setupLayer = [&](int idx, const LayerSettings& settings) {
					initialData.layers[idx].adaptedLuminance = 0.3f;
					initialData.layers[idx].targetLuminance = settings.targetLuminance;
					initialData.layers[idx].minExposure = settings.minExposure;
					initialData.layers[idx].maxExposure = settings.maxExposure;
					initialData.layers[idx].useAutoExposure = settings.autoExposureEnabled ? 1 : 0;
					initialData.layers[idx].centerWeightTightness = settings.centerWeightTightness;
					initialData.layers[idx].focusPoint = settings.focusPoint;
					initialData.layers[idx].histogramLowCutoff = settings.histogramLowCutoff;
					initialData.layers[idx].histogramHighCutoff = settings.histogramHighCutoff;
					initialData.layers[idx].speedUp = settings.speedUp;
					initialData.layers[idx].speedDown = settings.speedDown;

					initialData.layers[idx].autoTuneEnabled = settings.autoTuneEnabled ? 1 : 0;
					initialData.layers[idx].minContrast = settings.minContrast;
					initialData.layers[idx].maxContrast = settings.maxContrast;
					initialData.layers[idx].targetBrightness = settings.targetBrightness;

					initialData.layers[idx].uchimuraP = settings.uchimuraP;
					initialData.layers[idx].uchimuraA = settings.uchimuraA;
					initialData.layers[idx].uchimuraM = settings.uchimuraM;
					initialData.layers[idx].uchimuraL = settings.uchimuraL;
					initialData.layers[idx].uchimuraC = settings.uchimuraC;
					initialData.layers[idx].uchimuraB = settings.uchimuraB;
					initialData.layers[idx].toneMapMode = settings.toneMappingMode;
					initialData.layers[idx].toneMappingEnabled = settings.toneMappingEnabled ? 1 : 0;

					initialData.layers[idx].cdlSlope = glm::vec4(settings.cdlSlope, 0.0f);
					initialData.layers[idx].cdlOffset = glm::vec4(settings.cdlOffset, 0.0f);
					initialData.layers[idx].cdlPower = glm::vec4(settings.cdlPower, 0.0f);
					initialData.layers[idx].cdlSaturation = settings.cdlSaturation;

					initialData.layers[idx].whiteTemp = settings.whiteTemp;
					initialData.layers[idx].whiteTint = settings.whiteTint;

					initialData.layers[idx].ltmEnabled = settings.ltmEnabled ? 1 : 0;
					initialData.layers[idx].ltmEvSpread = settings.ltmEvSpread;
					initialData.layers[idx].ltmTarget = settings.ltmTarget;
					initialData.layers[idx].ltmSigma = settings.ltmSigma;
					initialData.layers[idx].ltmWeightContrast = settings.ltmWeightContrast;
					initialData.layers[idx].ltmWeightSaturation = settings.ltmWeightSaturation;
					initialData.layers[idx].ltmWeightExposedness = settings.ltmWeightExposedness;
					initialData.layers[idx].ltmBoostLocalContrast = settings.ltmBoostLocalContrast;
				};

				setupLayer(0, _sceneSettings);
				setupLayer(1, _skySettings);

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
			auto updateLayer = [&](int idx, const LayerSettings& settings) {
				float actualTarget = settings.targetLuminance * (idx == 0 ? (1.0f - _nightFactor * 0.5f) : 1.0f);
				float actualMax = settings.maxExposure * (idx == 0 ? (1.0f - _nightFactor * 0.4f) : 1.0f);

				size_t offset = idx * sizeof(LayerData);
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

				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, uchimuraP), sizeof(float), &settings.uchimuraP);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, uchimuraA), sizeof(float), &settings.uchimuraA);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, uchimuraM), sizeof(float), &settings.uchimuraM);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, uchimuraL), sizeof(float), &settings.uchimuraL);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, uchimuraC), sizeof(float), &settings.uchimuraC);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, uchimuraB), sizeof(float), &settings.uchimuraB);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, toneMapMode), sizeof(int), &settings.toneMappingMode);
				int tmEnabled = settings.toneMappingEnabled ? 1 : 0;
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset + offsetof(LayerData, toneMappingEnabled), sizeof(int), &tmEnabled);

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

			updateLayer(0, _sceneSettings);
			updateLayer(1, _skySettings);

			// 2. Compute-based Downsample, Bright Pass and Auto-Exposure
			_downsampleComputeShader->use();
			_downsampleComputeShader->setVec2("srcResolution", (float)_width, (float)_height);
			_downsampleComputeShader->setInt("numMips", _numMips);
			_downsampleComputeShader->setFloat("threshold", threshold_);
			_downsampleComputeShader->setFloat("deltaTime", _deltaTime);
			_downsampleComputeShader->setMat4("invView", glm::inverse(viewMatrix));
			_downsampleComputeShader->setMat4("invProjection", glm::inverse(projectionMatrix));

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, depthTexture); // Provide your G-buffer depth here


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

			// Unbind image units so they don't leak into later passes
			for (int i = 5; i < 20; i++) {
				glBindImageTexture(i, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
			}

			// 2.5 LTM Fusion
			if (_sceneSettings.ltmEnabled) {
				_ltmFuseComputeShader->use();
				_ltmFuseComputeShader->setInt("expTexture", 0);
				_ltmFuseComputeShader->setInt("wgtTexture", 1);
				_ltmFuseComputeShader->setInt("startMip", _numMips - 1);
				_ltmFuseComputeShader->setInt("endMip", 0);

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, _ltmExpTexture);
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, _ltmWgtTexture);
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
			_compositeShader->setMat4("invView", glm::inverse(viewMatrix));
			_compositeShader->setMat4("invProjection", glm::inverse(projectionMatrix));


			_compositeShader->setFloat("farPlane", Constants::Project::Camera::DefaultFarPlane());
			_compositeShader->setFloat("nearPlane", Constants::Project::Camera::DefaultNearPlane());

			GLuint lighting_idx = glGetUniformBlockIndex(_compositeShader->ID, "Lighting");
			if (lighting_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(_compositeShader->ID, lighting_idx, Constants::UboBinding::Lighting());
			}

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
			glBindTexture(GL_TEXTURE_2D, depthTexture); // Provide your G-buffer depth here

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

	using namespace PostProcessing;

	void BloomEffect::ApplyTargetState(const state::SystemConfiguration& config) {
		const auto& s = config.bloom;
		SetEnabled(s.enabled);
		SetIntensity(s.intensity);
		SetThreshold(s.threshold);

		auto applyLayer = [](BloomEffect::LayerSettings& dest, const state::BloomLayerSettings& src) {
			dest.toneMappingEnabled = src.toneMappingEnabled;
			dest.toneMappingMode = src.toneMappingMode;
			dest.autoExposureEnabled = src.autoExposureEnabled;
			dest.targetLuminance = src.targetLuminance;
			dest.minExposure = src.minExposure;
			dest.maxExposure = src.maxExposure;
			dest.speedUp = src.speedUp;
			dest.speedDown = src.speedDown;
			dest.centerWeightTightness = src.centerWeightTightness;
			dest.focusPoint = src.focusPoint;
			dest.histogramLowCutoff = src.histogramLowCutoff;
			dest.histogramHighCutoff = src.histogramHighCutoff;
			dest.uchimuraP = src.uchimuraP;
			dest.uchimuraA = src.uchimuraA;
			dest.uchimuraM = src.uchimuraM;
			dest.uchimuraL = src.uchimuraL;
			dest.uchimuraC = src.uchimuraC;
			dest.uchimuraB = src.uchimuraB;
			dest.autoTuneEnabled = src.autoTuneEnabled;
			dest.minContrast = src.minContrast;
			dest.maxContrast = src.maxContrast;
			dest.targetBrightness = src.targetBrightness;
			dest.cdlSlope = src.cdlSlope;
			dest.cdlOffset = src.cdlOffset;
			dest.cdlPower = src.cdlPower;
			dest.cdlSaturation = src.cdlSaturation;
			dest.whiteTemp = src.whiteTemp;
			dest.whiteTint = src.whiteTint;
			dest.ltmEnabled = src.ltmEnabled;
			dest.ltmEvSpread = src.ltmEvSpread;
			dest.ltmTarget = src.ltmTarget;
			dest.ltmSigma = src.ltmSigma;
			dest.ltmWeightContrast = src.ltmWeightContrast;
			dest.ltmWeightSaturation = src.ltmWeightSaturation;
			dest.ltmWeightExposedness = src.ltmWeightExposedness;
			dest.ltmBoostLocalContrast = src.ltmBoostLocalContrast;
		};

		applyLayer(_sceneSettings, s.scene);
		applyLayer(_skySettings, s.sky);
	}

} // namespace Boidsish
