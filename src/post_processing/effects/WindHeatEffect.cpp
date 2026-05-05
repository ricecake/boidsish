#include "post_processing/effects/WindHeatEffect.h"
#include "shader.h"
#include "constants.h"
#include <GL/glew.h>

namespace Boidsish {
	namespace PostProcessing {

		WindHeatEffect::WindHeatEffect() {
			name_ = "Wind & Heat";
			is_enabled_ = false;
		}

		WindHeatEffect::~WindHeatEffect() {}

		void WindHeatEffect::Initialize(int width, int height) {
			shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/post_processing/wind_heat.frag");
		}

		void WindHeatEffect::Apply(
			GLuint           sourceTexture,
			GLuint           depthTexture,
			GLuint           velocityTexture,
			GLuint           normalTexture,
			GLuint           albedoTexture,
			const glm::mat4& viewMatrix,
			const glm::mat4& projectionMatrix,
			const glm::vec3& cameraPos
		) {
			shader_->use();
			shader_->setFloat("u_time", time_);
			shader_->setFloat("u_windLineIntensity", wind_line_intensity_);
			shader_->setFloat("u_heatShimmerIntensity", heat_shimmer_intensity_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, depthTexture);

			// Bind noise texture if available
			if (noise_texture_ != 0) {
				glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::NoiseSimplex());
				glBindTexture(GL_TEXTURE_3D, noise_texture_);
			}

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void WindHeatEffect::Resize(int width, int height) {
			// No-op
		}

	} // namespace PostProcessing
} // namespace Boidsish
