#pragma once

#include <memory>
#include <vector>

#include "post_processing/IPostProcessingEffect.h"
#include <glm/glm.hpp>

// Forward declarations
class Shader;
class ComputeShader;

namespace Boidsish {
	namespace PostProcessing {

		class BloomEffect: public IPostProcessingEffect {
		public:
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

			void SetToneMappingEnabled(bool enabled) { _toneMappingEnabled = enabled; }
			bool IsToneMappingEnabled() const { return _toneMappingEnabled; }
			void SetToneMappingMode(int mode) { _toneMappingMode = mode; }
			int GetToneMappingMode() const { return _toneMappingMode; }

			void SetAutoExposureEnabled(bool enabled) { _autoExposureEnabled = enabled; }
			bool IsAutoExposureEnabled() const { return _autoExposureEnabled; }

			void  SetTargetLuminance(float target) { _targetLuminance = target; }
			float GetTargetLuminance() const { return _targetLuminance; }

			void  SetExposureLimits(float min, float max) { _minExposure = min; _maxExposure = max; }
			float GetMinExposure() const { return _minExposure; }
			float GetMaxExposure() const { return _maxExposure; }

			void  SetAdaptationSpeeds(float up, float down) { _speedUp = up; _speedDown = down; }
			float GetSpeedUp() const { return _speedUp; }
			float GetSpeedDown() const { return _speedDown; }

			void SetAutoExposureCenterWeight(float weight) { _centerWeightTightness = weight; }
			float GetAutoExposureCenterWeight() const { return _centerWeightTightness; }

			void SetAutoExposureFocusPoint(const glm::vec2& focus) { _focusPoint = focus; }
			glm::vec2 GetAutoExposureFocusPoint() const { return _focusPoint; }

			void SetHistogramCutoffs(float low, float high) { _histogramLowCutoff = low; _histogramHighCutoff = high; }
			float GetHistogramLowCutoff() const { return _histogramLowCutoff; }
			float GetHistogramHighCutoff() const { return _histogramHighCutoff; }

			void SetUchimuraParams(float P, float a, float m, float l, float c, float b) {
				_uchimuraP = P; _uchimuraA = a; _uchimuraM = m; _uchimuraL = l; _uchimuraC = c; _uchimuraB = b;
			}
			float GetUchimuraP() const { return _uchimuraP; }
			float GetUchimuraA() const { return _uchimuraA; }
			float GetUchimuraM() const { return _uchimuraM; }
			float GetUchimuraL() const { return _uchimuraL; }
			float GetUchimuraC() const { return _uchimuraC; }
			float GetUchimuraB() const { return _uchimuraB; }

			void SetNightFactor(float factor) override { _nightFactor = factor; }

			void SetTime(float time) override;

		private:
			void InitializeResources();

			std::unique_ptr<ComputeShader> _downsampleComputeShader;
			std::unique_ptr<Shader>        _upsampleShader;
			std::unique_ptr<Shader>        _compositeShader;

			GLuint _bloomTexture; // Mipmapped texture
			int    _numMips;
			std::vector<GLuint> _upsampleFBOs;

			GLuint _exposureSsbo = 0;

			int   _width, _height;
			float intensity_ = 0.075f;
			float threshold_ = 1.0f;
			float minIntensity_ = 0.05f;
			float maxIntensity_ = 0.150f;

			bool _toneMappingEnabled = true;
			int  _toneMappingMode = 2;

			bool  _autoExposureEnabled = true;
			float _targetLuminance = 0.25f;
			float _minExposure = 0.01f;
			float _maxExposure = 25.0f;
			float _speedUp = 3.0f;
			float _speedDown = 1.0f;

			float _centerWeightTightness = 4.0f;
			glm::vec2 _focusPoint = glm::vec2(0.5f, 0.5f);
			float _histogramLowCutoff = 0.1f;
			float _histogramHighCutoff = 0.95f;

			float _uchimuraP = 1.0f;
			float _uchimuraA = 1.0f;
			float _uchimuraM = 0.22f;
			float _uchimuraL = 0.4f;
			float _uchimuraC = 1.33f;
			float _uchimuraB = 0.0f;

			float _nightFactor = 0.0f;
			float _lastTime = 0.0f;
			float _deltaTime = 0.0f;
		};

	} // namespace PostProcessing
} // namespace Boidsish
