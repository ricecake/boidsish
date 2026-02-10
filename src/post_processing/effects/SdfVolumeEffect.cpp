#include "post_processing/effects/SdfVolumeEffect.h"

#include "constants.h"

namespace Boidsish {
	namespace PostProcessing {

		SdfVolumeEffect::SdfVolumeEffect() {
			name_ = "SdfVolume";
		}

		SdfVolumeEffect::~SdfVolumeEffect() {}

		void SdfVolumeEffect::Initialize(int width, int height) {
			shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/sdf_volume.frag");
			width_ = width;
			height_ = height;
		}

		void SdfVolumeEffect::Apply(const PostProcessingParams& params) {
			if (!shader_ || !shader_->isValid())
				return;

			shader_->use();
			shader_->setInt("sceneTexture", 0);
			shader_->setInt("depthTexture", 1);
			shader_->setVec2("screenSize", glm::vec2(width_, height_));
			shader_->setVec3("cameraPos", params.cameraPos);
			shader_->setMat4("invView", params.invViewMatrix);
			shader_->setMat4("invProjection", params.invProjectionMatrix);
			shader_->setFloat("time", time_);

			// Bind SDF volumes UBO
			GLuint blockIndex = glGetUniformBlockIndex(shader_->ID, "SdfVolumes");
			if (blockIndex != GL_INVALID_INDEX) {
				glUniformBlockBinding(shader_->ID, blockIndex, Constants::UboBinding::SdfVolumes());
			}

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, params.sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, params.depthTexture);

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void SdfVolumeEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
		}

	} // namespace PostProcessing
} // namespace Boidsish
