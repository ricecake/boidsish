#include "post_processing/effects/ScreenSpaceWeatherEffect.h"
#include "shader.h"
#include "constants.h"

namespace Boidsish {
	namespace PostProcessing {

		ScreenSpaceWeatherEffect::ScreenSpaceWeatherEffect() {
			name_ = "Screen Space Weather";
			is_enabled_ = false;
		}

		ScreenSpaceWeatherEffect::~ScreenSpaceWeatherEffect() {}

		void ScreenSpaceWeatherEffect::Initialize(int /*width*/, int /*height*/) {
			shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/screenspace_weather.frag");
			shader_->bindUniformBlock("VisualEffects", Constants::UboBinding::VisualEffects());
			shader_->bindUniformBlock("WindData", Constants::UboBinding::WindData());
		}

		void ScreenSpaceWeatherEffect::Apply(
			GLuint sourceTexture,
			GLuint depthTexture,
			GLuint velocityTexture,
			GLuint normalTexture,
			GLuint albedoTexture,
			const glm::mat4& viewMatrix,
			const glm::mat4& projectionMatrix,
			const glm::vec3& cameraPos
		) {
			shader_->use();
			shader_->setInt("sceneTexture", 0);
			shader_->setInt("depthTexture", 1);
			shader_->setInt("velocityTexture", 2);
			shader_->setInt("normalTexture", 3);
			shader_->setInt("albedoTexture", 4);
			shader_->setFloat("time", time_);

			// Matrices and vectors
			shader_->setMat4("viewMatrix", viewMatrix);
			shader_->setMat4("projectionMatrix", projectionMatrix);
			shader_->setMat4("invView", glm::inverse(viewMatrix));
			shader_->setMat4("invProjection", glm::inverse(projectionMatrix));
			shader_->setVec3("cameraPos", cameraPos);

			// Wind textures
			shader_->setInt("u_windTexture", Constants::TextureUnit::WindData());

			// Effect control uniforms
			shader_->setFloat("u_HeatStrength", heat_strength_);
			shader_->setFloat("u_HeatScale", heat_scale_);
			shader_->setFloat("u_HeatSpeed", heat_speed_);
			shader_->setFloat("u_HeatWidth", heat_width_);
			shader_->setFloat("u_HeatHeight", heat_height_);

			shader_->setFloat("u_WindAngle", wind_angle_);
			shader_->setFloat("u_WindSpeed", wind_speed_);
			shader_->setFloat("u_WindBlurScale", wind_blur_scale_);
			shader_->setFloat("u_WindGustFrequency", wind_gust_frequency_);
			shader_->setFloat("u_WindGustStrength", wind_gust_strength_);
			shader_->setFloat("u_WindRoughenStrength", wind_roughen_strength_);
			shader_->setFloat("u_WindStreakDecay", wind_streak_decay_);
			shader_->setFloat("u_WindTintStrength", wind_tint_strength_);

			shader_->setBool("u_UseManualIceCoverage", use_manual_ice_coverage_);
			shader_->setFloat("u_IceCoverage", ice_coverage_);
			shader_->setFloat("u_IceScale", ice_scale_);
			shader_->setFloat("u_IceEdgeWidth", ice_edge_width_);
			shader_->setVec3("u_IceColor", ice_color_);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, depthTexture);
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, velocityTexture);
			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, normalTexture);
			glActiveTexture(GL_TEXTURE4);
			glBindTexture(GL_TEXTURE_2D, albedoTexture);

			glDrawArrays(GL_TRIANGLES, 0, 6);

			// Reset active texture
			glActiveTexture(GL_TEXTURE0);
		}

		void ScreenSpaceWeatherEffect::Resize(int /*width*/, int /*height*/) {
			// No specific resizing needed for this effect
		}

	} // namespace PostProcessing
} // namespace Boidsish
