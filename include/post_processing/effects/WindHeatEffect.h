#pragma once

#include "post_processing/IPostProcessingEffect.h"
#include <memory>

#include "shader.h"

namespace Boidsish {

	namespace PostProcessing {

		class WindHeatEffect : public IPostProcessingEffect {
		public:
			WindHeatEffect();
			virtual ~WindHeatEffect();

			virtual void Apply(
				GLuint           sourceTexture,
				GLuint           depthTexture,
				GLuint           velocityTexture,
				GLuint           normalTexture,
				GLuint           albedoTexture,
				const glm::mat4& viewMatrix,
				const glm::mat4& projectionMatrix,
				const glm::vec3& cameraPos
			) override;

			virtual void Initialize(int width, int height) override;
			virtual void Resize(int width, int height) override;

			void  SetWindLineIntensity(float intensity) { wind_line_intensity_ = intensity; }
			float GetWindLineIntensity() const { return wind_line_intensity_; }

			void  SetHeatShimmerIntensity(float intensity) { heat_shimmer_intensity_ = intensity; }
			float GetHeatShimmerIntensity() const { return heat_shimmer_intensity_; }

			void SetNoiseTexture(GLuint noiseTexture) { noise_texture_ = noiseTexture; }

		private:
			std::unique_ptr<Shader> shader_;
			GLuint noise_texture_ = 0;
			float wind_line_intensity_ = 0.5f;
			float heat_shimmer_intensity_ = 0.5f;
			float time_ = 0.0f;

			virtual void SetTime(float time) override { time_ = time; }
		};

	} // namespace PostProcessing
} // namespace Boidsish
