#pragma once

#include "post_processing/IPostProcessingEffect.h"
#include <shader.h>
#include <memory>

namespace Boidsish {
	namespace PostProcessing {

		class VolumetricCloudEffect : public IPostProcessingEffect {
		public:
			VolumetricCloudEffect();
			virtual ~VolumetricCloudEffect();

			void Initialize(int width, int height) override;
			void Apply(
				GLuint           sourceTexture,
				GLuint           depthTexture,
				const glm::mat4& viewMatrix,
				const glm::mat4& projectionMatrix,
				const glm::vec3& cameraPos
			) override;
			void Resize(int width, int height) override;

			void SetTime(float time) override { time_ = time; }

		private:
			std::unique_ptr<Shader> shader_;
			int                     width_, height_;
			float                   time_ = 0.0f;
		};

	} // namespace PostProcessing
} // namespace Boidsish
