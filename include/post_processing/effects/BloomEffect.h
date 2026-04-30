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

		private:
			void InitializeResources();

			std::unique_ptr<Shader>        _brightPassShader;
			std::unique_ptr<ComputeShader> _downsampleComputeShader;
			std::unique_ptr<Shader>        _upsampleShader;
			std::unique_ptr<Shader>        _compositeShader;

			GLuint _brightPassFBO;
			GLuint _brightPassTexture;

			GLuint             _bloomTexture; // Mipmapped texture
			int                _numMips;
			std::vector<GLuint> _upsampleFBOs;

			int   _width, _height;
			float intensity_ = 0.075f;
			float threshold_ = 1.0f;
			float minIntensity_ = 0.05f;
			float maxIntensity_ = 0.150f;

			bool _toneMappingEnabled = false;
			int  _toneMappingMode = 2;
		};

	} // namespace PostProcessing
} // namespace Boidsish
