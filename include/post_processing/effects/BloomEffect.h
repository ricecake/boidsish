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

		struct LayerSettings {
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

			bool  toneMappingEnabled = true;
			int   toneMappingMode = 5; // Default to Uchimura
		};

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

			// Layer settings access
			LayerSettings& GetSceneSettings() { return _scene; }
			LayerSettings& GetSkySettings() { return _sky; }

			void SetNightFactor(float factor) override { _nightFactor = factor; }
			void SetTime(float time) override;

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
				float _pad_ld3;
				float _pad_ld4;

				// ASC CDL (vec4 for alignment)
				alignas(16) glm::vec4 cdlSlope;
				alignas(16) glm::vec4 cdlOffset;
				alignas(16) glm::vec4 cdlPower;
				float     cdlSaturation;

				// White Balance
				float whiteTemp;
				float whiteTint;
				float _pad_ld5;

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
				uint32_t workgroupCounter;
				uint32_t _pad_ae1;
				uint32_t _pad_ae2;
				uint32_t _pad_ae3;

				LayerData scene;
				LayerData sky;
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

			LayerSettings _scene;
			LayerSettings _sky;

			float _nightFactor = 0.0f;
			float _lastTime = 0.0f;
			float _deltaTime = 0.0f;
		};

	} // namespace PostProcessing
} // namespace Boidsish
