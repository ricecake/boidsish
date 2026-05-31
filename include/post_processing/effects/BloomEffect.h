#pragma once

#include <memory>
#include <vector>

#include "post_processing/IPostProcessingEffect.h"
#include <glm/glm.hpp>
#include "state.h"

// Forward declarations
class Shader;
class ComputeShader;

namespace Boidsish {
	namespace PostProcessing {

		class BloomEffect: public IPostProcessingEffect {
		public:
			struct LayerSettings {
				bool  toneMappingEnabled = true;
				int   toneMappingMode = 5; // Default to Uchimura

				bool  autoExposureEnabled = true;
				float targetLuminance = 0.25f;
				float minExposure = 0.01f;
				float maxExposure = 25.0f;
				float speedUp = 3.0f;
				float speedDown = 1.0f;

				float centerWeightTightness = 4.0f;
				glm::vec2 focusPoint = glm::vec2(0.5f, 0.5f);
				float histogramLowCutoff = 0.1f;
				float histogramHighCutoff = 0.95f;

				float uchimuraP = 1.0f;
				float uchimuraA = 1.0f;
				float uchimuraM = 0.22f;
				float uchimuraL = 0.4f;
				float uchimuraC = 1.33f;
				float uchimuraB = 0.0f;

				bool  autoTuneEnabled = true;
				float minContrast = 0.6f;
				float maxContrast = 1.3f;
				float targetBrightness = 1.0f;

				glm::vec3 cdlSlope = glm::vec3(1.0f);
				glm::vec3 cdlOffset = glm::vec3(0.0f);
				glm::vec3 cdlPower = glm::vec3(1.0f);
				float     cdlSaturation = 1.0f;

				float whiteTemp = 6500.0f;
				float whiteTint = 0.0f;

				bool  ltmEnabled = true;
				float ltmEvSpread = 2.0f;
				float ltmTarget = 0.5f;
				float ltmSigma = 0.2f;
				float ltmWeightContrast = 0.0f;
				float ltmWeightSaturation = 0.0f;
				float ltmWeightExposedness = 1.0f;
				float ltmBoostLocalContrast = 0.0f;
			};

			BloomEffect(int width, int height);
			~BloomEffect();

			void Initialize(int width, int height) override;
			void Apply(GLuint sourceTexture, GLuint depthTexture, GLuint velocityTexture, GLuint normalTexture, GLuint albedoTexture, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraPos) override;
			void Resize(int width, int height) override;

			void SetIntensity(float intensity) { intensity_ = intensity; }

			float GetIntensity() const { return intensity_; }

			void SetThreshold(float threshold) { threshold_ = threshold; }

			float GetThreshold() const { return threshold_; }

			void SetMinIntensity(float minIntensity) { minIntensity_ = minIntensity; }

			float GetMinIntensity() const { return minIntensity_; }

			void SetMaxIntensity(float maxIntensity) { maxIntensity_ = maxIntensity; }

			float GetMaxIntensity() const { return maxIntensity_; }

			LayerSettings& GetSceneSettings() { return _sceneSettings; }
			LayerSettings& GetSkySettings() { return _skySettings; }

			void SetToneMappingEnabled(bool enabled) { _sceneSettings.toneMappingEnabled = enabled; }
			bool IsToneMappingEnabled() const { return _sceneSettings.toneMappingEnabled; }
			void SetToneMappingMode(int mode) { _sceneSettings.toneMappingMode = mode; }
			int GetToneMappingMode() const { return _sceneSettings.toneMappingMode; }

			void SetAutoExposureEnabled(bool enabled) { _sceneSettings.autoExposureEnabled = enabled; }
			bool IsAutoExposureEnabled() const { return _sceneSettings.autoExposureEnabled; }

			void  SetTargetLuminance(float target) { _sceneSettings.targetLuminance = target; }
			float GetTargetLuminance() const { return _sceneSettings.targetLuminance; }

			void  SetExposureLimits(float min, float max) { _sceneSettings.minExposure = min; _sceneSettings.maxExposure = max; }
			float GetMinExposure() const { return _sceneSettings.minExposure; }
			float GetMaxExposure() const { return _sceneSettings.maxExposure; }

			void  SetAdaptationSpeeds(float up, float down) { _sceneSettings.speedUp = up; _sceneSettings.speedDown = down; }
			float GetSpeedUp() const { return _sceneSettings.speedUp; }
			float GetSpeedDown() const { return _sceneSettings.speedDown; }

			void SetAutoExposureCenterWeight(float weight) { _sceneSettings.centerWeightTightness = weight; }
			float GetAutoExposureCenterWeight() const { return _sceneSettings.centerWeightTightness; }

			void SetAutoExposureFocusPoint(const glm::vec2& focus) { _sceneSettings.focusPoint = focus; }
			glm::vec2 GetAutoExposureFocusPoint() const { return _sceneSettings.focusPoint; }

			void SetHistogramCutoffs(float low, float high) { _sceneSettings.histogramLowCutoff = low; _sceneSettings.histogramHighCutoff = high; }
			float GetHistogramLowCutoff() const { return _sceneSettings.histogramLowCutoff; }
			float GetHistogramHighCutoff() const { return _sceneSettings.histogramHighCutoff; }

			void SetUchimuraParams(float P, float a, float m, float l, float c, float b) {
				_sceneSettings.uchimuraP = P; _sceneSettings.uchimuraA = a; _sceneSettings.uchimuraM = m; _sceneSettings.uchimuraL = l; _sceneSettings.uchimuraC = c; _sceneSettings.uchimuraB = b;
			}
			float GetUchimuraP() const { return _sceneSettings.uchimuraP; }
			float GetUchimuraA() const { return _sceneSettings.uchimuraA; }
			float GetUchimuraM() const { return _sceneSettings.uchimuraM; }
			float GetUchimuraL() const { return _sceneSettings.uchimuraL; }
			float GetUchimuraC() const { return _sceneSettings.uchimuraC; }
			float GetUchimuraB() const { return _sceneSettings.uchimuraB; }

			void SetAutoTuneEnabled(bool enabled) { _sceneSettings.autoTuneEnabled = enabled; }
			bool IsAutoTuneEnabled() const { return _sceneSettings.autoTuneEnabled; }
			void SetAutoTuneConstraints(float minC, float maxC, float targetB) {
				_sceneSettings.minContrast = minC; _sceneSettings.maxContrast = maxC; _sceneSettings.targetBrightness = targetB;
			}
			float GetMinContrast() const { return _sceneSettings.minContrast; }
			float GetMaxContrast() const { return _sceneSettings.maxContrast; }
			float GetTargetBrightness() const { return _sceneSettings.targetBrightness; }

			void SetCdlParams(const glm::vec3& slope, const glm::vec3& offset, const glm::vec3& power, float saturation) {
				_sceneSettings.cdlSlope = slope; _sceneSettings.cdlOffset = offset; _sceneSettings.cdlPower = power; _sceneSettings.cdlSaturation = saturation;
			}
			glm::vec3 GetCdlSlope() const { return _sceneSettings.cdlSlope; }
			glm::vec3 GetCdlOffset() const { return _sceneSettings.cdlOffset; }
			glm::vec3 GetCdlPower() const { return _sceneSettings.cdlPower; }
			float GetCdlSaturation() const { return _sceneSettings.cdlSaturation; }

			void SetWhiteBalance(float temp, float tint) { _sceneSettings.whiteTemp = temp; _sceneSettings.whiteTint = tint; }
			float GetWhiteTemp() const { return _sceneSettings.whiteTemp; }
			float GetWhiteTint() const { return _sceneSettings.whiteTint; }

			void SetLtmEnabled(bool enabled) { _sceneSettings.ltmEnabled = enabled; }
			bool IsLtmEnabled() const { return _sceneSettings.ltmEnabled; }

			void SetLtmParams(float evSpread, float target, float sigma) {
				_sceneSettings.ltmEvSpread = evSpread; _sceneSettings.ltmTarget = target; _sceneSettings.ltmSigma = sigma;
			}
			float GetLtmEvSpread() const { return _sceneSettings.ltmEvSpread; }
			float GetLtmTarget() const { return _sceneSettings.ltmTarget; }
			float GetLtmSigma() const { return _sceneSettings.ltmSigma; }

			void SetLtmWeights(float contrast, float saturation, float exposedness) {
				_sceneSettings.ltmWeightContrast = contrast; _sceneSettings.ltmWeightSaturation = saturation; _sceneSettings.ltmWeightExposedness = exposedness;
			}
			float GetLtmWeightContrast() const { return _sceneSettings.ltmWeightContrast; }
			float GetLtmWeightSaturation() const { return _sceneSettings.ltmWeightSaturation; }
			float GetLtmWeightExposedness() const { return _sceneSettings.ltmWeightExposedness; }

			void SetLtmBoostLocalContrast(float boost) { _sceneSettings.ltmBoostLocalContrast = boost; }
			float GetLtmBoostLocalContrast() const { return _sceneSettings.ltmBoostLocalContrast; }

			void SetNightFactor(float factor) override { _nightFactor = factor; }

			void SetTime(float time) override;

			void ApplyTargetState(const class state::SystemConfiguration& config);
			void SyncState();

			struct LayerData {
				float adaptedLuminance;
				float targetLuminance;
				float minExposure;
				float maxExposure;

				int   useAutoExposure;
				float centerWeightTightness;
				glm::vec2 focusPoint;

				float histogramLowCutoff;
				float histogramHighCutoff;
				float speedUp;
				float speedDown;

				// Statistics
				float minLuma;
				float maxLuma;
				float avgLuma;
				float stdDevLuma;

				// EMAs
				float emaMinLuma;
				float emaMaxLuma;
				float emaAvgLuma;
				float emaStdDevLuma;

				// Auto-tune settings
				int   autoTuneEnabled;
				float minContrast;
				float maxContrast;
				float targetBrightness;

				// Auto-calculated Uchimura parameters
				float autoUchimuraP;
				float autoUchimuraA;
				float autoUchimuraM;
				float autoUchimuraL;
				float autoUchimuraC;
				float autoUchimuraB;
				float _pad0;
				float _pad1;

				// Manual Uchimura parameters
				float uchimuraP;
				float uchimuraA;
				float uchimuraM;
				float uchimuraL;
				float uchimuraC;
				float uchimuraB;
				int   toneMapMode;
				int   toneMappingEnabled;

				// ASC CDL (vec4 for alignment)
				glm::vec4 cdlSlope;
				glm::vec4 cdlOffset;
				glm::vec4 cdlPower;
				float     cdlSaturation;

				// White Balance
				float whiteTemp;
				float whiteTint;
				float _pad2;

				// Local Tone Mapping (Exposure Fusion)
				int   ltmEnabled;
				float ltmEvSpread;
				float ltmTarget;
				float ltmSigma;

				float ltmWeightContrast;
				float ltmWeightSaturation;
				float ltmWeightExposedness;
				float ltmBoostLocalContrast;

				uint32_t histogram[256];
			};

			struct ExposureData {
				LayerData layers[2];
				uint32_t  workgroupCounter;
			};

		private:
			void InitializeResources();

			std::unique_ptr<ComputeShader> _downsampleComputeShader;
			std::unique_ptr<Shader>        _upsampleShader;
			std::unique_ptr<Shader>        _compositeShader;

			GLuint _bloomTexture; // Mipmapped texture
			GLuint _ltmExpTexture; // Synthetic exposure lightness
			GLuint _ltmWgtTexture; // Synthetic exposure weights
			GLuint _ltmFusedTexture; // Fused LTM result
			int    _numMips;
			std::vector<GLuint> _upsampleFBOs;

			std::unique_ptr<ComputeShader> _ltmFuseComputeShader;

			GLuint _exposureSsbo = 0;

			int   _width, _height;
			float intensity_ = 0.075f;
			float threshold_ = 1.0f;
			float minIntensity_ = 0.05f;
			float maxIntensity_ = 0.150f;

			LayerSettings _sceneSettings;
			LayerSettings _skySettings = { .targetLuminance = 0.5 };

			float _nightFactor = 0.0f;
			float _lastTime = 0.0f;
			float _deltaTime = 0.0f;
		};

	} // namespace PostProcessing
} // namespace Boidsish
