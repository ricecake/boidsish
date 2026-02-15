#include "post_processing/PostProcessingManager.h"
#include "post_processing/effects/AtmosphereEffect.h"

#include "shader.h"

namespace Boidsish {
	namespace PostProcessing {

		AtmosphereEffect::AtmosphereEffect() {
			name_ = "Atmosphere";
		}

		AtmosphereEffect::~AtmosphereEffect() {}

		void AtmosphereEffect::Initialize(int width, int height) {
			shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/atmosphere.frag");

			GLuint lighting_idx = glGetUniformBlockIndex(shader_->ID, "Lighting");
			if (lighting_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(shader_->ID, lighting_idx, 0);
			}

			width_ = width;
			height_ = height;
		}

		void AtmosphereEffect::Apply(
			GLuint           sourceTexture,
			const GBuffer&   gbuffer,
			const glm::mat4& viewMatrix,
			const glm::mat4& projectionMatrix,
			const glm::vec3& cameraPos
		) {
			shader_->use();
			shader_->setInt("sceneTexture", 0);
			shader_->setInt("gbuffer.depth", 1);
			shader_->setFloat("time", time_);
			shader_->setVec3("cameraPos", cameraPos);
			shader_->setMat4("invView", glm::inverse(viewMatrix));
			shader_->setMat4("invProjection", glm::inverse(projectionMatrix));

			shader_->setFloat("hazeDensity", haze_density_);
			shader_->setFloat("hazeHeight", haze_height_);
			shader_->setVec3("hazeColor", haze_color_);
			shader_->setFloat("cloudDensity", cloud_density_);
			shader_->setFloat("cloudAltitude", cloud_altitude_);
			shader_->setFloat("cloudThickness", cloud_thickness_);
			shader_->setVec3("cloudColorUniform", cloud_color_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, gbuffer.depth);

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void AtmosphereEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
		}

	} // namespace PostProcessing
} // namespace Boidsish
