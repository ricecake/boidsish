#include "post_processing/effects/ToneMappingEffect.h"

#include "shader.h"

namespace Boidsish {
	namespace PostProcessing {

		ToneMappingEffect::ToneMappingEffect() {
			name_ = "ToneMapping";
		}

		ToneMappingEffect::~ToneMappingEffect() {}

		void ToneMappingEffect::Initialize(int width, int height) {
			_shader = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/tonemapping.frag");
			width_ = width;
			height_ = height;
		}

		void ToneMappingEffect::Apply(
			GLuint sourceTexture,
			GLuint depthTexture,
			GLuint /* velocityTexture */, GLuint normalTexture, GLuint materialTexture,
			const glm::mat4& viewMatrix,
			const glm::mat4& projectionMatrix,
			const glm::vec3& cameraPos
		) {
			_shader->use();
			_shader->setInt("sceneTexture", 0);
			_shader->setInt("toneMapMode", toneMode_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void ToneMappingEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
		}

	} // namespace PostProcessing
} // namespace Boidsish
