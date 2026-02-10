#include "post_processing/effects/SssrEffect.h"
#include "shader.h"
#include <GL/glew.h>

namespace Boidsish {
	namespace PostProcessing {

		SssrEffect::SssrEffect() {
			name_ = "SSSR";
			is_enabled_ = true;
		}

		SssrEffect::~SssrEffect() = default;

		void SssrEffect::Initialize(int width, int height) {
			width_ = width;
			height_ = height;
			shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/sssr.frag");
		}

		void SssrEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
		}

		void SssrEffect::Apply(const PostProcessingParams& params) {
			shader_->use();
			shader_->setInt("sceneTexture", 0);
			shader_->setInt("depthTexture", 1);
			shader_->setInt("normalTexture", 2);
			shader_->setInt("pbrTexture", 3);
			shader_->setMat4("view", params.viewMatrix);
			shader_->setMat4("projection", params.projectionMatrix);
			shader_->setMat4("invProjection", params.invProjectionMatrix);
			shader_->setMat4("invView", params.invViewMatrix);
			shader_->setFloat("time", params.time);
			shader_->setVec3("viewPos", params.cameraPos);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, params.sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, params.depthTexture);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, params.normalTexture);
			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, params.pbrTexture);

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

	} // namespace PostProcessing
} // namespace Boidsish
