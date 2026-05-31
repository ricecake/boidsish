#include "post_processing/effects/VolumetricLightingEffect.h"
#include "constants.h"
#include "service_locator.h"
#include "shadow_manager.h"
#include "light_manager.h"
#include "terrain_render_manager.h"
#include "atmosphere_manager.h"
#include "fire_effect_manager.h"
#include "NoiseManager.h"
#include "weather_manager.h"
#include <GL/glew.h>
#include "state.h"

namespace Boidsish {
	namespace PostProcessing {

		VolumetricLightingEffect::VolumetricLightingEffect() {
			name_ = "Volumetric Lighting";
		}

		VolumetricLightingEffect::~VolumetricLightingEffect() {
			if (injection_texture_) glDeleteTextures(1, &injection_texture_);
			if (scattering_texture_) glDeleteTextures(1, &scattering_texture_);
			if (history_textures_[0]) glDeleteTextures(2, history_textures_);
		}

		void VolumetricLightingEffect::Initialize(int width, int height) {
			width_ = width;
			height_ = height;

			injection_shader_ = std::make_unique<ComputeShader>("shaders/effects/volumetric_injection.comp");
			integration_shader_ = std::make_unique<ComputeShader>("shaders/effects/volumetric_integration.comp");
			composite_shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/volumetric_composite.frag");

			auto setup_shader = [](ShaderBase& s) {
				s.use();
				s.bindUniformBlock("Lighting", Constants::UboBinding::Lighting());
				s.bindUniformBlock("Shadows", Constants::UboBinding::Shadows());
				s.bindUniformBlock("TerrainData", Constants::UboBinding::TerrainData());
				s.bindUniformBlock("FrustumData", Constants::UboBinding::FrustumData());
				s.bindUniformBlock("TemporalData", Constants::UboBinding::TemporalData());
			};

			setup_shader(*injection_shader_);
			setup_shader(*integration_shader_);
			setup_shader(*composite_shader_);

			CreateGridTextures();
		}

		void VolumetricLightingEffect::Resize(int width, int height) {
			width_ = width;
			height_ = height;
		}

		void VolumetricLightingEffect::CreateGridTextures() {
			if (injection_texture_) glDeleteTextures(1, &injection_texture_);
			if (scattering_texture_) glDeleteTextures(1, &scattering_texture_);
			if (history_textures_[0]) glDeleteTextures(2, history_textures_);

			auto create3DArray = [&](GLuint& tex) {
				glGenTextures(1, &tex);
				glBindTexture(GL_TEXTURE_3D, tex);
				glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, grid_res_x_, grid_res_y_, grid_res_z_ * num_cascades_, 0, GL_RGBA, GL_FLOAT, nullptr);
				glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
			};

			create3DArray(injection_texture_);
			create3DArray(scattering_texture_);
			create3DArray(history_textures_[0]);
			create3DArray(history_textures_[1]);

			has_history_ = false;
		}

		void VolumetricLightingEffect::PreDispatch(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraPos) {
			auto& loc = ServiceLocator::Instance();
			auto shadow_mgr = loc.Get<ShadowManager>();
			auto terrain_mgr = loc.Get<TerrainRenderManager>();
			auto light_mgr = loc.Get<LightManager>();
			auto atmos_mgr = loc.Get<AtmosphereManager>();
			auto fire_mgr = loc.Get<FireEffectManager>();
			auto noise_mgr = loc.Get<NoiseManager>();
			auto wm = loc.Get<WeatherManager>();

			auto weather = wm->GetCurrentWeather();

			// 1. Injection
			injection_shader_->use();
			if (shadow_mgr) shadow_mgr->BindForRendering(*injection_shader_);
			if (terrain_mgr) terrain_mgr->BindTerrainData(*injection_shader_);
			if (atmos_mgr) atmos_mgr->BindToShader(*injection_shader_);
			if (fire_mgr) fire_mgr->BindBuffers(*injection_shader_);

			if (noise_mgr) {
				glActiveTexture(GL_TEXTURE10);
				glBindTexture(GL_TEXTURE_2D, noise_mgr->GetBlueNoiseTexture());
				injection_shader_->setInt("uBlueNoise", 10);

				glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::NoiseCurl());
				glBindTexture(GL_TEXTURE_3D, noise_mgr->GetCurlTexture());
				injection_shader_->setInt("u_curlTexture", Constants::TextureUnit::NoiseCurl());
			}

			injection_shader_->setFloat("uAnisotropy", anisotropy_);
			injection_shader_->setFloat("uIntensity", intensity_);
			injection_shader_->setMat4("uInvView", glm::inverse(viewMatrix));
			injection_shader_->setMat4("uInvProj", glm::inverse(projectionMatrix));
			injection_shader_->setMat4("uPrevVP", prev_view_projection_);
			injection_shader_->setVec3("uPrevCamPos", prev_camera_pos_);
			injection_shader_->setVec3("uPrevCamFront", prev_camera_front_);
			injection_shader_->setFloat("uTemporalAlpha", has_history_ ? temporal_alpha_ : 0.0f);

			injection_shader_->setFloat("u_cell_size", Constants::Class::Particles::ParticleGridCellSize());
			injection_shader_->setUint("u_grid_size", Constants::Class::Particles::ParticleGridSize());
			injection_shader_->setIVec3("u_grid_res", glm::ivec3(grid_res_x_, grid_res_y_, grid_res_z_));
			injection_shader_->setFloat("hazeDensity", weather.haze_density);
			injection_shader_->setFloat("hazeHeight", weather.haze_height);
			injection_shader_->setVec3("hazeColor", weather.haze_color);

			glBindImageTexture(Constants::ImageBinding::VolumetricInjection(), injection_texture_, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);

			glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::VolumetricHistory());
			glBindTexture(GL_TEXTURE_3D, history_textures_[history_index_]);
			injection_shader_->setInt("uHistoryTexture", Constants::TextureUnit::VolumetricHistory());

			glDispatchCompute((grid_res_x_ + 7) / 8, (grid_res_y_ + 7) / 8, (grid_res_z_ * num_cascades_ + 3) / 4);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			// 2. Integration (Accumulate along Z)
			integration_shader_->use();
			integration_shader_->setIVec3("u_grid_res", glm::ivec3(grid_res_x_, grid_res_y_, grid_res_z_));

			glBindImageTexture(Constants::ImageBinding::VolumetricInjection(), injection_texture_, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA16F);
			glBindImageTexture(Constants::ImageBinding::VolumetricScattering(), scattering_texture_, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);

			// Copy to history for next frame
			int next_history = 1 - history_index_;
			glBindImageTexture(Constants::ImageBinding::VolumetricHistory(), history_textures_[next_history], 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);

			glDispatchCompute(grid_res_x_, grid_res_y_, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			history_index_ = next_history;
			has_history_ = true;
			prev_view_projection_ = projectionMatrix * viewMatrix;
			prev_camera_pos_ = cameraPos;
			// Extract camera front from inverse view matrix (3rd column is Back)
			glm::mat4 invView = glm::inverse(viewMatrix);
			prev_camera_front_ = -glm::normalize(glm::vec3(invView[2]));
		}

		void VolumetricLightingEffect::ApplyTargetState(const state::VolumetricSettings& config) {
			SetEnabled(config.enabled);
			SetIntensity(config.intensity);
			SetScatteringAnisotropy(config.anisotropy);
			SetTemporalAlpha(config.temporalAlpha);
		}

		void VolumetricLightingEffect::Apply(GLuint sourceTexture, GLuint depthTexture, GLuint velocityTexture, GLuint normalTexture, GLuint albedoTexture, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraPos) {
			// Composition
			composite_shader_->use();
			composite_shader_->setInt("uSceneTexture", 0);
			composite_shader_->setInt("uDepthTexture", 1);
			composite_shader_->setInt("uVolumetricTexture", Constants::TextureUnit::VolumetricScattering());

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sourceTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, depthTexture);
			glActiveTexture(GL_TEXTURE0 + Constants::TextureUnit::VolumetricScattering());
			glBindTexture(GL_TEXTURE_3D, scattering_texture_);

			glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

	} // namespace PostProcessing
} // namespace Boidsish
