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

			bool _toneMappingEnabled = false;
			int  _toneMappingMode = 2;

			bool  _autoExposureEnabled = true;
			float _targetLuminance = 0.5f;
			float _minExposure = 0.1f;
			float _maxExposure = 10.0f;
			float _speedUp = 3.0f;
			float _speedDown = 1.0f;
			float _nightFactor = 0.0f;
			float _lastTime = 0.0f;
			float _deltaTime = 0.0f;
		};

	} // namespace PostProcessing
} // namespace Boidsish
