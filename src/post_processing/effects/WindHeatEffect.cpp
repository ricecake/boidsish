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

			shader_->setMat4("invView", glm::inverse(viewMatrix));
			shader_->setMat4("invProjection", glm::inverse(projectionMatrix));

			shader_->setInt("u_windTexture", Constants::TextureUnit::WindData());
			shader_->setInt("u_weatherScalarsTexture", Constants::TextureUnit::WeatherScalars());

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			shader_->setInt("u_screenTexture", 0);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, depthTexture);
			shader_->setInt("u_depthTexture", 1);

			// Wind and Weather textures are bound globally by WeatherManager in Visualizer::Render
			// but we need to ensure their sampler uniforms are set if they are not using tokens.
			// wind.glsl uses tokens [[WIND_TEXTURE_BINDING]] and [[WEATHER_SCALARS_BINDING]].

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void WindHeatEffect::Resize(int width, int height) {
			// No-op
		}

	} // namespace PostProcessing
} // namespace Boidsish
