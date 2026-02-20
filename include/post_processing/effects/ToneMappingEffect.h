#pragma once

#include <memory>

#include "post_processing/IPostProcessingEffect.h"
#include "shader.h"

namespace Boidsish {
	namespace PostProcessing {

		class ToneMappingEffect: public IPostProcessingEffect {
		public:
			ToneMappingEffect();
			~ToneMappingEffect();

			void Initialize(int width, int height) override;
			void Apply(
				GLuint           sourceTexture,
				GLuint           depthTexture,
				GLuint velocityTexture, GLuint normalTexture, GLuint materialTexture,
				const glm::mat4& viewMatrix,
				const glm::mat4& projectionMatrix,
				const glm::vec3& cameraPos
			) override;
			void Resize(int width, int height) override;

			void SetMode(float toneMode) { toneMode_ = toneMode; }

			int GetMode() const { return toneMode_; }

		private:
			std::unique_ptr<Shader> _shader;
			int                     width_ = 0;
			int                     height_ = 0;
			int                     toneMode_ = 2;
		};

	} // namespace PostProcessing
} // namespace Boidsish
