#pragma once

#include <memory>
#include <vector>

#include "post_processing/IPostProcessingEffect.h"

class Shader; // Forward declaration

namespace Boidsish {
	namespace PostProcessing {

		class SsaoEffect: public IPostProcessingEffect {
		public:
			SsaoEffect();
			~SsaoEffect();

			void Apply(
				GLuint           sourceTexture,
				const GBuffer&   gbuffer,
				const glm::mat4& viewMatrix,
				const glm::mat4& projectionMatrix,
				const glm::vec3& cameraPos
			) override;
			void Initialize(int width, int height) override;
			void Resize(int width, int height) override;

			bool IsEarly() const override { return true; }

			// Configuration options
			void SetRadius(float radius) { radius_ = radius; }

			float GetRadius() const { return radius_; }

			void SetBias(float bias) { bias_ = bias; }

			float GetBias() const { return bias_; }

			void SetIntensity(float intensity) { intensity_ = intensity; }

			float GetIntensity() const { return intensity_; }

			void SetPower(float power) { power_ = power; }

			float GetPower() const { return power_; }

		private:
			void InitializeFBOs();
			void GenerateKernel();
			void GenerateNoiseTexture();

			std::unique_ptr<Shader> ssao_shader_;
			std::unique_ptr<Shader> blur_shader_;
			std::unique_ptr<Shader> composite_shader_;

			GLuint ssao_fbo_ = 0;
			GLuint ssao_texture_ = 0;
			GLuint blur_fbo_ = 0;
			GLuint blur_texture_ = 0;
			GLuint noise_texture_ = 0;

			std::vector<glm::vec3> ssao_kernel_;

			float radius_ = 0.5f;
			float bias_ = 0.1f;
			float intensity_ = 1.0f;
			float power_ = 1.0f;

			int width_ = 0;
			int height_ = 0;
		};

	} // namespace PostProcessing
} // namespace Boidsish
