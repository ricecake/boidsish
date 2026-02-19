#include "post_processing/effects/AtmosphereEffect.h"

#include "constants.h"
#include "shader.h"

namespace Boidsish {
	namespace PostProcessing {

		AtmosphereEffect::AtmosphereEffect() {
			name_ = "Atmosphere";
		}

		AtmosphereEffect::~AtmosphereEffect() {}

		void AtmosphereEffect::Initialize(int width, int height) {
			shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/atmosphere.frag");

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

			// Explicitly set standard sampler bindings
			shader_->setInt("shadowMaps", 4);

			width_ = width;
			height_ = height;
		}

		void AtmosphereEffect::Apply(
			GLuint           sourceTexture,
			GLuint           depthTexture,
			GLuint           /* velocityTexture */,
			const glm::mat4& viewMatrix,
			const glm::mat4& projectionMatrix,
			const glm::vec3& cameraPos
		) {
			shader_->use();
			shader_->setInt("sceneTexture", 0);
			shader_->setInt("depthTexture", 1);
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
			glBindTexture(GL_TEXTURE_2D, depthTexture);

			glDrawArrays(GL_TRIANGLES, 0, 6);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, 0);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		void AtmosphereEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
		}

	} // namespace PostProcessing
} // namespace Boidsish
