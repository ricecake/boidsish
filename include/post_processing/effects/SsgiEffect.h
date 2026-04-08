#ifndef SSGI_EFFECT_H
#define SSGI_EFFECT_H

#include <memory>
#include <vector>

#include "post_processing/IPostProcessingEffect.h"
#include "post_processing/TemporalAccumulator.h"
#include <glm/glm.hpp>

class Shader;
class ComputeShader;

namespace Boidsish {
	namespace PostProcessing {

		class SsgiEffect: public IPostProcessingEffect {
		public:
			SsgiEffect();
			~SsgiEffect();

			void Initialize(int width, int height) override;
			void Apply(GLuint sourceTexture, GLuint depthTexture, GLuint velocityTexture, GLuint normalTexture, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraPos) override;
			void Resize(int width, int height) override;

			void SetIntensity(float intensity) { intensity_ = intensity; }
			float GetIntensity() const { return intensity_; }

			void SetRadius(float radius) { radius_ = radius; }
			float GetRadius() const { return radius_; }

			void SetDistanceFalloff(float falloff) { distance_falloff_ = falloff; }
			float GetDistanceFalloff() const { return distance_falloff_; }

			void SetSteps(int steps) { steps_ = steps; }
			int GetSteps() const { return steps_; }

			void SetRayCount(int rays) { ray_count_ = rays; }
			int GetRayCount() const { return ray_count_; }

			void SetNoiseTextures(GLuint blueNoise) { blue_noise_texture_ = blueNoise; }
			void SetHiZTexture(GLuint hiz, int mips) { hiz_texture_ = hiz; hiz_mips_ = mips; }

		private:
			void InitializeTextures();

			std::unique_ptr<ComputeShader> ssgi_shader_;
			std::unique_ptr<Shader>        composite_shader_;
			TemporalAccumulator            temporal_accumulator_;

			GLuint ssgi_texture_ = 0;
			GLuint blue_noise_texture_ = 0;
			GLuint hiz_texture_ = 0;
			int    hiz_mips_ = 0;
			int    width_ = 0, height_ = 0;

			float intensity_ = 1.0f;
			float radius_ = 2.0f;
			float distance_falloff_ = 1.0f;
			int   steps_ = 8;
			int   ray_count_ = 2;
		};

	} // namespace PostProcessing
} // namespace Boidsish

#endif // SSGI_EFFECT_H
