#ifndef SCREEN_SPACE_SHADOW_EFFECT_H
#define SCREEN_SPACE_SHADOW_EFFECT_H

#include "post_processing/IPostProcessingEffect.h"
#include "post_processing/TemporalAccumulator.h"
#include <memory>
#include <GL/glew.h>

class Shader;
class ComputeShader;

namespace Boidsish {
	namespace PostProcessing {

		class ScreenSpaceShadowEffect : public IPostProcessingEffect {
		public:
			ScreenSpaceShadowEffect();
			~ScreenSpaceShadowEffect();

			void Initialize(int width, int height) override;
			void Apply(GLuint sourceTexture, GLuint depthTexture, GLuint velocityTexture, GLuint normalTexture, GLuint albedoTexture, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraPos) override;
			void Resize(int width, int height) override;

			bool IsEarly() const override { return true; }

			void SetIntensity(float intensity) { intensity_ = intensity; }
			float GetIntensity() const { return intensity_; }

			void SetRadius(float radius) { radius_ = radius; }
			float GetRadius() const { return radius_; }

			void SetBias(float bias) { bias_ = bias; }
			float GetBias() const { return bias_; }

			void SetSteps(int steps) { steps_ = steps; }
			int GetSteps() const { return steps_; }

			void SetNoiseTextures(GLuint blueNoise) { blue_noise_texture_ = blueNoise; }
			GLuint GetShadowMaskTexture() const { return temporal_accumulator_.GetResult(); }

		private:
			void InitializeTextures();

			std::unique_ptr<ComputeShader> sss_shader_;
			std::unique_ptr<Shader>        composite_shader_;
			TemporalAccumulator            temporal_accumulator_;

			GLuint shadow_mask_texture_ = 0;
			GLuint blue_noise_texture_ = 0;
			int width_ = 0;
			int height_ = 0;

			float intensity_ = 0.25f;
			float radius_ = 1.25f;
			float bias_ = 0.5f;
			int steps_ = 8;
		};

	} // namespace PostProcessing
} // namespace Boidsish

#endif // SCREEN_SPACE_SHADOW_EFFECT_H
