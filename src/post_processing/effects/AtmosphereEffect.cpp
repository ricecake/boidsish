#include "post_processing/effects/AtmosphereEffect.h"

#include "ConfigManager.h"
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

			scattering_.Initialize();

			width_ = width;
			height_ = height;
		}

		void AtmosphereEffect::Apply(
			GLuint           sourceTexture,
			GLuint           depthTexture,
			const glm::mat4& viewMatrix,
			const glm::mat4& projectionMatrix,
			const glm::vec3& cameraPos
		) {
			auto& config = ConfigManager::GetInstance();

			// Update scattering parameters with current world scale and config
			auto params = scattering_.GetParameters();

			params.world_scale = world_scale_;
			params.rayleigh_multiplier = config.GetAppSettingFloat("atmosphere_density", 1.0f);
			params.mie_multiplier = config.GetAppSettingFloat("fog_density", 1.0f);
			params.mie_anisotropy = config.GetAppSettingFloat("mie_anisotropy", 0.80f);
			params.sun_intensity_factor = config.GetAppSettingFloat("sun_intensity_factor", 15.0f);
			params.rayleigh_scale_height = config.GetAppSettingFloat("rayleigh_scale_height", 100.0f);
			params.mie_scale_height = config.GetAppSettingFloat("mie_scale_height", 15.0f);
			params.bottom_radius = config.GetAppSettingFloat("planet_radius", 79500.0f);
			params.top_radius = config.GetAppSettingFloat("atmosphere_top", 80750.0f);

			scattering_.Update(params);

			shader_->use();
			shader_->setInt("sceneTexture", 0);
			shader_->setInt("depthTexture", 1);
			shader_->setFloat("time", time_);
			shader_->setVec3("cameraPos", cameraPos);
			shader_->setMat4("invView", glm::inverse(viewMatrix));
			shader_->setMat4("invProjection", glm::inverse(projectionMatrix));

			shader_->setFloat("cloudDensity", config.GetAppSettingFloat("cloud_density", 0.2f));
			shader_->setFloat("cloudAltitude", config.GetAppSettingFloat("cloud_altitude", 2.0f));
			shader_->setFloat("cloudThickness", config.GetAppSettingFloat("cloud_thickness", 0.5f));
			shader_->setVec3("cloudColorUniform", cloud_color_); // Still use member for now, or move to config

			shader_->setBool("enableClouds", config.GetAppSettingBool("enable_clouds", true));
			shader_->setBool("enableFog", config.GetAppSettingBool("enable_fog", true));

			// Atmosphere scattering parameters
			const auto& p = scattering_.GetParameters();
			shader_->setVec3("rayleighScattering", p.rayleigh_scattering * p.rayleigh_multiplier);
			shader_->setFloat("rayleighScaleHeight", p.rayleigh_scale_height * p.world_scale);
			shader_->setFloat("mieScattering", p.mie_scattering * p.mie_multiplier);
			shader_->setFloat("mieExtinction", p.mie_extinction * p.mie_multiplier);
			shader_->setFloat("mieScaleHeight", p.mie_scale_height * p.world_scale);
			shader_->setFloat("mieAnisotropy", p.mie_anisotropy);
			shader_->setVec3("absorptionExtinction", p.absorption_extinction);
			shader_->setFloat("bottomRadius", p.bottom_radius * p.world_scale);
			shader_->setFloat("topRadius", p.top_radius * p.world_scale);
			shader_->setFloat("sunIntensity", p.sun_intensity);
			shader_->setFloat("sunIntensityFactor", p.sun_intensity_factor);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, depthTexture);

			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, scattering_.GetTransmittanceLUT());
			shader_->setInt("transmittanceLUT", 2);

			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, scattering_.GetMultiScatteringLUT());
			shader_->setInt("multiScatteringLUT", 3);

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		void AtmosphereEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
		}

	} // namespace PostProcessing
} // namespace Boidsish
