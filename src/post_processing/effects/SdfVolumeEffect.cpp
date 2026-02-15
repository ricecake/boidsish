#include "post_processing/PostProcessingManager.h"
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

		void SdfVolumeEffect::Apply(
			GLuint           sourceTexture,
			const GBuffer&   gbuffer,
			const glm::mat4& viewMatrix,
			const glm::mat4& projectionMatrix,
			const glm::vec3& cameraPos
		) {
			if (!shader_ || !shader_->isValid())
				return;

			shader_->use();
			shader_->setInt("sceneTexture", 0);
			shader_->setInt("gbuffer.depth", 1);
			shader_->setVec2("screenSize", glm::vec2(width_, height_));
			shader_->setVec3("cameraPos", cameraPos);
			shader_->setMat4("invView", glm::inverse(viewMatrix));
			shader_->setMat4("invProjection", glm::inverse(projectionMatrix));
			shader_->setFloat("time", time_);

			// Bind SDF volumes UBO
			GLuint blockIndex = glGetUniformBlockIndex(shader_->ID, "SdfVolumes");
			if (blockIndex != GL_INVALID_INDEX) {
				glUniformBlockBinding(shader_->ID, blockIndex, Constants::UboBinding::SdfVolumes());
			}

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, gbuffer.depth);

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void SdfVolumeEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
		}

	} // namespace PostProcessing
} // namespace Boidsish
