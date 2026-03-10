#pragma once

#include <memory>
#include <vector>

#include "post_processing/IPostProcessingEffect.h"
#include <glm/glm.hpp>

// Forward declarations
class Shader;

namespace Boidsish {
	namespace PostProcessing {

		struct BloomMip {
			glm::vec2 size;
			GLuint    fbo;
			GLuint    texture;
		};

		class BloomEffect: public IPostProcessingEffect {
		public:
			BloomEffect(int width, int height);
			~BloomEffect();

			void Initialize(int width, int height) override;
			void Apply(
				GLuint           sourceTexture,
				GLuint           depthTexture,
				GLuint           velocityTexture,
				const glm::mat4& viewMatrix,
				const glm::mat4& projectionMatrix,
				const glm::vec3& cameraPos
			) override;
			void Resize(int width, int height) override;

			void SetIntensity(float intensity) { intensity_ = intensity; }

			float GetIntensity() const { return intensity_; }

			void SetThreshold(float threshold) { threshold_ = threshold; }

			float GetThreshold() const { return threshold_; }

			void  SetFlareIntensity(float intensity) { flareIntensity_ = intensity; }
			float GetFlareIntensity() const { return flareIntensity_; }

			void  SetFlareThreshold(float threshold) { flareThreshold_ = threshold; }
			float GetFlareThreshold() const { return flareThreshold_; }

			void  SetHorizontalFlareIntensity(float intensity) { horizontalFlareIntensity_ = intensity; }
			float GetHorizontalFlareIntensity() const { return horizontalFlareIntensity_; }

			void  SetVerticalFlareIntensity(float intensity) { verticalFlareIntensity_ = intensity; }
			float GetVerticalFlareIntensity() const { return verticalFlareIntensity_; }

		private:
			void InitializeFBOs();

			std::unique_ptr<Shader> _brightPassShader;
			std::unique_ptr<Shader> _downsampleShader;
			std::unique_ptr<Shader> _upsampleShader;
			std::unique_ptr<Shader> _compositeShader;
			std::unique_ptr<Shader> _streakShader;

			GLuint                _brightPassFBO;
			GLuint                _brightPassTexture;
			std::vector<BloomMip> _mipChain;

			// Flare resources
			GLuint _flareBrightPassFBO;
			GLuint _flareBrightPassTexture;

			GLuint _horizontalFlareFBO;
			GLuint _horizontalFlareTexture;

			GLuint _verticalFlareFBO;
			GLuint _verticalFlareTexture;

			GLuint _flarePingPongFBO;
			GLuint _flarePingPongTexture;

			int   _width, _height;
			float intensity_ = 0.15f;
			float threshold_ = 1.0f;

			float flareIntensity_ = 0.1f;
			float flareThreshold_ = 1.5f;
			float horizontalFlareIntensity_ = 0.75f;
			float verticalFlareIntensity_ = 0.25f;
		};

	} // namespace PostProcessing
} // namespace Boidsish
