#include "post_processing/effects/VoxelVolumeEffect.h"

#include "constants.h"

namespace Boidsish {
	namespace PostProcessing {

		VoxelVolumeEffect::VoxelVolumeEffect() {
			name_ = "VoxelVolume";
		}

		VoxelVolumeEffect::~VoxelVolumeEffect() {}

		void VoxelVolumeEffect::Initialize(int width, int height) {
			shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/voxel_volume.frag");

			shader_->use();
			GLuint lighting_idx = glGetUniformBlockIndex(shader_->ID, "Lighting");
			if (lighting_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(shader_->ID, lighting_idx, Constants::UboBinding::Lighting());
			}
			GLuint temporal_idx = glGetUniformBlockIndex(shader_->ID, "TemporalData");
			if (temporal_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(shader_->ID, temporal_idx, Constants::UboBinding::TemporalData());
			}

			// Voxel SSBOs and sampler should be globally managed, but let's ensure bindings here if needed
			// Actually, shader_table and Visualizer handle default bindings.

			width_ = width;
			height_ = height;
		}

		void VoxelVolumeEffect::Apply(
			GLuint sourceTexture,
			GLuint depthTexture,
			GLuint /* velocityTexture */,
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

			// u_brickPool is on texture unit 13
			shader_->setInt("u_brickPool", 13);

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

		void VoxelVolumeEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
		}

	} // namespace PostProcessing
} // namespace Boidsish
