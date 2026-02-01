#include "post_processing/effects/VolumetricCloudEffect.h"
#include "constants.h"

namespace Boidsish {
	namespace PostProcessing {

		VolumetricCloudEffect::VolumetricCloudEffect() {
			name_ = "VolumetricClouds";
		}

		VolumetricCloudEffect::~VolumetricCloudEffect() {}

		void VolumetricCloudEffect::Initialize(int width, int height) {
			shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/volumetric_clouds.frag");
			width_ = width;
			height_ = height;
		}

		void VolumetricCloudEffect::Apply(
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
			shader_->setMat4("invView", glm::inverse(viewMatrix));
			shader_->setMat4("invProjection", glm::inverse(projectionMatrix));

			// Bind SDF volumes UBO
			GLuint blockIndex = glGetUniformBlockIndex(shader_->ID, "SdfVolumes");
			if (blockIndex != GL_INVALID_INDEX) {
				glUniformBlockBinding(shader_->ID, blockIndex, Constants::UboBinding::SdfVolumes());
			}

			// Bind Lighting UBO
			GLuint lightingIndex = glGetUniformBlockIndex(shader_->ID, "Lighting");
			if (lightingIndex != GL_INVALID_INDEX) {
				glUniformBlockBinding(shader_->ID, lightingIndex, Constants::UboBinding::Lighting());
			}

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, depthTexture);

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void VolumetricCloudEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
		}

	} // namespace PostProcessing
} // namespace Boidsish
