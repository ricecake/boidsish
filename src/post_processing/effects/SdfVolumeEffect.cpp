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

			shader_->use();
			GLuint lighting_idx = glGetUniformBlockIndex(shader_->ID, "Lighting");
			if (lighting_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(shader_->ID, lighting_idx, Constants::UboBinding::Lighting());
			}
			GLuint shadows_idx = glGetUniformBlockIndex(shader_->ID, "Shadows");
			if (shadows_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(shader_->ID, shadows_idx, Constants::UboBinding::Shadows());
			}
			GLuint effects_idx = glGetUniformBlockIndex(shader_->ID, "VisualEffects");
			if (effects_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(shader_->ID, effects_idx, Constants::UboBinding::VisualEffects());
			}
			GLuint sdf_volumes_idx = glGetUniformBlockIndex(shader_->ID, "SdfVolumes");
			if (sdf_volumes_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(shader_->ID, sdf_volumes_idx, Constants::UboBinding::SdfVolumes());
			}

			width_ = width;
			height_ = height;
		}

		void SdfVolumeEffect::Apply(
			GLuint           sourceTexture,
			GLuint           depthTexture,
			const glm::mat4& viewMatrix,
			const glm::mat4& projectionMatrix,
			const glm::vec3& cameraPos
		) {
			if (!shader_ || !shader_->isValid())
				return;

			shader_->use();
			shader_->setInt("sceneTexture", 0);
			shader_->setInt("depthTexture", 1);
			shader_->setVec2("screenSize", glm::vec2(width_, height_));
			shader_->setVec3("cameraPos", cameraPos);
			shader_->setMat4("invView", glm::inverse(viewMatrix));
			shader_->setMat4("invProjection", glm::inverse(projectionMatrix));
			shader_->setFloat("time", time_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, depthTexture);

			glDrawArrays(GL_TRIANGLES, 0, 6);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, 0);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		void SdfVolumeEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
		}

	} // namespace PostProcessing
} // namespace Boidsish
