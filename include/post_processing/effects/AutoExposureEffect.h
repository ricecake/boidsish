#pragma once

#include <memory>

#include "post_processing/IPostProcessingEffect.h"
#include "shader.h"

namespace Boidsish {
	namespace PostProcessing {

		class AutoExposureEffect: public IPostProcessingEffect {
		public:
			AutoExposureEffect();
			~AutoExposureEffect();

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

			void SetSpeedUp(float s) { speedUp_ = s; }

			float GetSpeedUp() const { return speedUp_; }

			void SetSpeedDown(float s) { speedDown_ = s; }

			float GetSpeedDown() const { return speedDown_; }

			void SetTargetLuminance(float t) { targetLuminance_ = t; }

			float GetTargetLuminance() const { return targetLuminance_; }

			void SetMinExposure(float m) { minExposure_ = m; }

			float GetMinExposure() const { return minExposure_; }

			void SetMaxExposure(float m) { maxExposure_ = m; }

			float GetMaxExposure() const { return maxExposure_; }

			void SetEnabled(bool enabled) override;
			void SetTime(float time) override;

		private:
			std::unique_ptr<ComputeShader> computeShader_;
			std::unique_ptr<Shader>        passthroughShader_;
			GLuint                         exposureSsbo_ = 0;
			float                          speedUp_ = 3.0f;
			float                          speedDown_ = 1.0f;
			float                          targetLuminance_ = 0.3f;
			float                          minExposure_ = 0.01f;
			float                          maxExposure_ = 2.5f;
			float                          deltaTime_ = 0.016f;
			float                          lastTime_ = 0.0f;
		};

	} // namespace PostProcessing
} // namespace Boidsish
