#pragma once

#include "post_processing/IPostProcessingEffect.h"
#include <memory>
#include <shader.h>

namespace Boidsish {
	namespace PostProcessing {

		class SssEffect : public IPostProcessingEffect {
		public:
			SssEffect();
			virtual ~SssEffect() override;

			virtual void Initialize(int width, int height) override;
			virtual void Apply(GLuint sourceTexture, GLuint depthTexture, GLuint velocityTexture,
							   const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix,
							   const glm::vec3& cameraPos) override;
			virtual void Resize(int width, int height) override;
			virtual bool IsEarly() const override { return true; }

		private:
			std::unique_ptr<Shader> sss_shader_;
			int width_, height_;
		};

	}
}
