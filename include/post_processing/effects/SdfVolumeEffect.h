#pragma once

#include <memory>

#include "post_processing/IPostProcessingEffect.h"
#include "shader.h"

namespace Boidsish {
	namespace PostProcessing {

		class SdfVolumeEffect: public IPostProcessingEffect {
		public:
			SdfVolumeEffect();
			virtual ~SdfVolumeEffect();

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

			void SetTime(float time) override { time_ = time; }

			void SetSdfData(const glm::vec3& min, const glm::vec3& max, int pos, int neg) {
				sdfMin_ = min;
				sdfMax_ = max;
				numPositive_ = pos;
				numNegative_ = neg;
			}

			bool IsEarly() const override { return true; }

		private:
			std::unique_ptr<Shader> shader_;
			int                     width_;
			int                     height_;
			float                   time_ = 0.0f;

			glm::vec3 sdfMin_{0.0f};
			glm::vec3 sdfMax_{0.0f};
			int       numPositive_ = 0;
			int       numNegative_ = 0;
		};

	} // namespace PostProcessing
} // namespace Boidsish
