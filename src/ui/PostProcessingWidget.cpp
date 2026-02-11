#include "ui/PostProcessingWidget.h"

#include "ConfigManager.h"
#include "imgui.h"
#include "post_processing/effects/AtmosphereEffect.h"
#include "post_processing/effects/AutoExposureEffect.h"
#include "post_processing/effects/BloomEffect.h"
#include "post_processing/effects/FilmGrainEffect.h"
#include "post_processing/effects/SsaoEffect.h"
#include "post_processing/effects/ToneMappingEffect.h"

namespace Boidsish {
	namespace UI {

		PostProcessingWidget::PostProcessingWidget(PostProcessing::PostProcessingManager& manager): manager_(manager) {}

		void PostProcessingWidget::Draw() {
			ImGui::Begin("Post-Processing Effects");

			for (auto& effect : manager_.GetPreToneMappingEffects()) {
				bool is_enabled = effect->IsEnabled();
				if (ImGui::Checkbox(effect->GetName().c_str(), &is_enabled)) {
					effect->SetEnabled(is_enabled);
				}

				if (effect->GetName() == "AutoExposure" && is_enabled) {
					auto auto_exposure_effect = std::dynamic_pointer_cast<PostProcessing::AutoExposureEffect>(effect);
					if (auto_exposure_effect) {
						float speed_up = auto_exposure_effect->GetSpeedUp();
						if (ImGui::SliderFloat("Speed Up", &speed_up, 0.1f, 10.0f)) {
							auto_exposure_effect->SetSpeedUp(speed_up);
						}
						float speed_down = auto_exposure_effect->GetSpeedDown();
						if (ImGui::SliderFloat("Speed Down", &speed_down, 0.1f, 10.0f)) {
							auto_exposure_effect->SetSpeedDown(speed_down);
						}
						float target_lum = auto_exposure_effect->GetTargetLuminance();
						if (ImGui::SliderFloat("Target Luminance", &target_lum, 0.01f, 1.0f)) {
							auto_exposure_effect->SetTargetLuminance(target_lum);
						}
					}
				}

				if (effect->GetName() == "Film Grain" && is_enabled) {
					auto film_grain_effect = std::dynamic_pointer_cast<PostProcessing::FilmGrainEffect>(effect);
					if (film_grain_effect) {
						float intensity = film_grain_effect->GetIntensity();
						if (ImGui::SliderFloat("Intensity", &intensity, 0.0f, 1.0f)) {
							film_grain_effect->SetIntensity(intensity);
						}
					}
				}

				if (effect->GetName() == "Atmosphere" && is_enabled) {
					auto  atmosphere_effect = std::dynamic_pointer_cast<PostProcessing::AtmosphereEffect>(effect);
					auto& config = ConfigManager::GetInstance();

					if (atmosphere_effect) {
						if (ImGui::Button("Reset to Earth Defaults")) {
							auto defaults = PostProcessing::AtmosphereScattering::Parameters();
							config.SetFloat("atmosphere_density", defaults.rayleigh_multiplier);
							config.SetFloat("fog_density", defaults.mie_multiplier);
							config.SetFloat("mie_anisotropy", defaults.mie_anisotropy);
							config.SetFloat("sun_intensity_factor", defaults.sun_intensity_factor);
							config.SetFloat("rayleigh_scale_height", defaults.rayleigh_scale_height);
							config.SetFloat("mie_scale_height", defaults.mie_scale_height);
							config.SetFloat("planet_radius", defaults.bottom_radius);
							config.SetFloat("atmosphere_top", defaults.top_radius);
							// RGB coefficients are usually constant but we allow overrides
							atmosphere_effect->GetScattering().Update(defaults);
						}

						ImGui::Separator();
						ImGui::Text("Global Toggles");
						bool clouds_enabled = config.GetAppSettingBool("enable_clouds", true);
						if (ImGui::Checkbox("Enable Clouds", &clouds_enabled)) {
							config.SetBool("enable_clouds", clouds_enabled);
						}
						bool fog_enabled = config.GetAppSettingBool("enable_fog", true);
						if (ImGui::Checkbox("Enable Fog", &fog_enabled)) {
							config.SetBool("enable_fog", fog_enabled);
						}

						if (ImGui::CollapsingHeader("Atmosphere Scattering", ImGuiTreeNodeFlags_DefaultOpen)) {
							float atmosphere_density = config.GetAppSettingFloat("atmosphere_density", 1.0f);
							if (ImGui::SliderFloat("Atmosphere Density", &atmosphere_density, 0.0f, 5.0f)) {
								config.SetFloat("atmosphere_density", atmosphere_density);
							}

							float fog_density = config.GetAppSettingFloat("fog_density", 1.0f);
							if (ImGui::SliderFloat("Fog Density", &fog_density, 0.0f, 10.0f)) {
								config.SetFloat("fog_density", fog_density);
							}

							float mie_anisotropy = config.GetAppSettingFloat("mie_anisotropy", 0.80f);
							if (ImGui::SliderFloat("Mie Anisotropy (G)", &mie_anisotropy, 0.0f, 0.99f)) {
								config.SetFloat("mie_anisotropy", mie_anisotropy);
							}

							float sun_intensity_factor = config.GetAppSettingFloat("sun_intensity_factor", 15.0f);
							if (ImGui::SliderFloat("Sun Intensity Factor", &sun_intensity_factor, 0.1f, 50.0f)) {
								config.SetFloat("sun_intensity_factor", sun_intensity_factor);
							}
						}

						if (ImGui::CollapsingHeader("Clouds", ImGuiTreeNodeFlags_DefaultOpen)) {
							float cloud_density = config.GetAppSettingFloat("cloud_density", 0.2f);
							if (ImGui::SliderFloat("Cloud Density", &cloud_density, 0.0f, 1.0f)) {
								config.SetFloat("cloud_density", cloud_density);
							}
							float cloud_altitude = config.GetAppSettingFloat("cloud_altitude", 2.0f);
							if (ImGui::SliderFloat("Cloud Altitude", &cloud_altitude, 0.1f, 10.0f)) {
								config.SetFloat("cloud_altitude", cloud_altitude);
							}
							float cloud_thickness = config.GetAppSettingFloat("cloud_thickness", 0.5f);
							if (ImGui::SliderFloat("Cloud Thickness", &cloud_thickness, 0.1f, 5.0f)) {
								config.SetFloat("cloud_thickness", cloud_thickness);
							}
							glm::vec3 cloud_color = atmosphere_effect->GetCloudColor();
							if (ImGui::ColorEdit3("Cloud Color", &cloud_color[0])) {
								atmosphere_effect->SetCloudColor(cloud_color);
							}
						}

						if (ImGui::CollapsingHeader("Advanced Physical Parameters")) {
							auto& params = atmosphere_effect->GetScattering().GetParameters();
							auto  new_params = params;
							bool  changed = false;

							ImGui::Text("Scattering Coefficients (Frequency Dependent)");
							if (ImGui::DragFloat3(
									"Rayleigh RGB",
									&new_params.rayleigh_scattering[0],
									0.00001f,
									0.0f,
									0.01f,
									"%.6f"
								))
								changed = true;

							if (ImGui::DragFloat(
									"Mie Scattering",
									&new_params.mie_scattering,
									0.00001f,
									0.0f,
									0.01f,
									"%.6f"
								))
								changed = true;

							ImGui::Separator();
							ImGui::Text("Scale Heights");
							float rayleigh_scale_height = config.GetAppSettingFloat("rayleigh_scale_height", 100.0f);
							if (ImGui::SliderFloat("Rayleigh H", &rayleigh_scale_height, 10.0f, 500.0f)) {
								config.SetFloat("rayleigh_scale_height", rayleigh_scale_height);
							}

							float mie_scale_height = config.GetAppSettingFloat("mie_scale_height", 15.0f);
							if (ImGui::SliderFloat("Mie H", &mie_scale_height, 1.0f, 100.0f)) {
								config.SetFloat("mie_scale_height", mie_scale_height);
							}

							ImGui::Separator();
							ImGui::Text("Spatial Geometry");
							float planet_radius = config.GetAppSettingFloat("planet_radius", 79500.0f);
							if (ImGui::DragFloat("Planet Radius", &planet_radius, 10.0f, 1000.0f, 200000.0f)) {
								config.SetFloat("planet_radius", planet_radius);
							}

							float atmosphere_top = config.GetAppSettingFloat("atmosphere_top", 80750.0f);
							if (ImGui::DragFloat("Atmosphere Top", &atmosphere_top, 10.0f, 1000.0f, 210000.0f)) {
								config.SetFloat("atmosphere_top", atmosphere_top);
							}

							if (ImGui::ColorEdit3("Ground Albedo", &new_params.ground_albedo[0]))
								changed = true;

							if (changed) {
								atmosphere_effect->GetScattering().Update(new_params);
							}
						}
					}
				}

				if (effect->GetName() == "SSAO" && is_enabled) {
					auto ssao_effect = std::dynamic_pointer_cast<PostProcessing::SsaoEffect>(effect);
					if (ssao_effect) {
						float radius = ssao_effect->GetRadius();
						if (ImGui::SliderFloat("Radius", &radius, 0.01f, 2.0f)) {
							ssao_effect->SetRadius(radius);
						}
						float bias = ssao_effect->GetBias();
						if (ImGui::SliderFloat("Bias", &bias, 0.001f, 0.1f)) {
							ssao_effect->SetBias(bias);
						}
						float intensity = ssao_effect->GetIntensity();
						if (ImGui::SliderFloat("Intensity##SSAO", &intensity, 0.0f, 5.0f)) {
							ssao_effect->SetIntensity(intensity);
						}
						float power = ssao_effect->GetPower();
						if (ImGui::SliderFloat("Power##SSAO", &power, 0.1f, 5.0f)) {
							ssao_effect->SetPower(power);
						}
					}
				}

				if (effect->GetName() == "Bloom" && is_enabled) {
					auto bloom_effect = std::dynamic_pointer_cast<PostProcessing::BloomEffect>(effect);
					if (bloom_effect) {
						float intensity = bloom_effect->GetIntensity();
						if (ImGui::SliderFloat("Intensity##Bloom", &intensity, 0.0f, 2.0f)) {
							bloom_effect->SetIntensity(intensity);
						}
						float threshold = bloom_effect->GetThreshold();
						if (ImGui::SliderFloat("Threshold", &threshold, 0.0f, 3.0f)) {
							bloom_effect->SetThreshold(threshold);
						}
					}
				}
			}

			if (auto effect = manager_.GetToneMappingEffect()) {
				auto tone_mapping_effect = std::dynamic_pointer_cast<PostProcessing::ToneMappingEffect>(effect);
				ImGui::Separator();
				bool is_enabled = tone_mapping_effect->IsEnabled();
				if (is_enabled) {
					// Camera mode dropdown
					const char* modes[] =
						{"ACES", "Filmic", "Lottes", "Reinhard", "Reinhard II", "Uchimura", "Uncharted 2", "Unreal 3"};
					int current_mode = tone_mapping_effect->GetMode();
					if (ImGui::Combo("Mode", &current_mode, modes, IM_ARRAYSIZE(modes))) {
						tone_mapping_effect->SetMode(current_mode);
					}
				}
			}

			ImGui::End();
		}

	} // namespace UI
} // namespace Boidsish
