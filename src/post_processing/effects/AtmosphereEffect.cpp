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

		void AtmosphereEffect::Apply(const PostProcessingParams& params) {
			shader_->use();
			shader_->setInt("sceneTexture", 0);
			shader_->setInt("depthTexture", 1);
			shader_->setFloat("time", time_);
			shader_->setVec3("cameraPos", params.cameraPos);
			shader_->setMat4("invView", params.invViewMatrix);
			shader_->setMat4("invProjection", params.invProjectionMatrix);

			shader_->setFloat("hazeDensity", haze_density_);
			shader_->setFloat("hazeHeight", haze_height_);
			shader_->setVec3("hazeColor", haze_color_);
			shader_->setFloat("cloudDensity", cloud_density_);
			shader_->setFloat("cloudAltitude", cloud_altitude_);
			shader_->setFloat("cloudThickness", cloud_thickness_);
			shader_->setVec3("cloudColorUniform", cloud_color_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, params.sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, params.depthTexture);

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void AtmosphereEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
		}

	} // namespace PostProcessing
} // namespace Boidsish
