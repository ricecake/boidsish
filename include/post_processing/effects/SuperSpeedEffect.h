#pragma once

#include <memory>

#include "post_processing/IPostProcessingEffect.h"

class Shader;

namespace Boidsish {
	namespace PostProcessing {

		class SuperSpeedEffect: public IPostProcessingEffect {
		public:
			SuperSpeedEffect();
			~SuperSpeedEffect() override;

			void Apply(
				GLuint           sourceTexture,
				GLuint           depthTexture,
				GLuint velocityTexture, GLuint normalTexture, GLuint materialTexture,
				const glm::mat4& viewMatrix,
				const glm::mat4& projectionMatrix,
				const glm::vec3& cameraPos
			) override;
			void Initialize(int width, int height) override;
			void Resize(int width, int height) override;

			void SetIntensity(float intensity) { intensity_ = intensity; }

			float GetIntensity() const { return intensity_; }

			void SetTime(float time) override { time_ = time; }

		private:
			std::unique_ptr<Shader> shader_;
			float                   intensity_ = 0.0f;
			float                   time_ = 0.0f;
			int                     width_, height_;
		};

	} // namespace PostProcessing
} // namespace Boidsish
