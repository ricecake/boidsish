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
				const GBuffer&   gbuffer,
				const glm::mat4& viewMatrix,
				const glm::mat4& projectionMatrix,
				const glm::vec3& cameraPos
			) override;
			void Resize(int width, int height) override;

			void SetIntensity(float intensity) { intensity_ = intensity; }

			float GetIntensity() const { return intensity_; }

			void SetThreshold(float threshold) { threshold_ = threshold; }

			float GetThreshold() const { return threshold_; }

		private:
			void InitializeFBOs();

			std::unique_ptr<Shader> _brightPassShader;
			std::unique_ptr<Shader> _downsampleShader;
			std::unique_ptr<Shader> _upsampleShader;
			std::unique_ptr<Shader> _compositeShader;

			GLuint                _brightPassFBO;
			GLuint                _brightPassTexture;
			std::vector<BloomMip> _mipChain;

			int   _width, _height;
			float intensity_ = 0.1f;
			float threshold_ = 1.0f;
		};

	} // namespace PostProcessing
} // namespace Boidsish
