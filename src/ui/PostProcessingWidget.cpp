#include "ui/PostProcessingWidget.h"

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
					auto atmosphere_effect = std::dynamic_pointer_cast<PostProcessing::AtmosphereEffect>(effect);
					if (atmosphere_effect) {
						bool clouds_enabled = atmosphere_effect->AreCloudsEnabled();
						if (ImGui::Checkbox("Enable Clouds", &clouds_enabled)) {
							atmosphere_effect->SetCloudsEnabled(clouds_enabled);
						}
						bool fog_enabled = atmosphere_effect->IsFogEnabled();
						if (ImGui::Checkbox("Enable Fog", &fog_enabled)) {
							atmosphere_effect->SetFogEnabled(fog_enabled);
						}

						if (ImGui::CollapsingHeader("Scattering Parameters")) {
							auto& params = atmosphere_effect->GetScattering().GetParameters();
							auto  new_params = params;

							if (ImGui::Button("Reset to Earth Defaults")) {
								new_params = PostProcessing::AtmosphereScattering::Parameters();
							}

							ImGui::Separator();
							ImGui::Text("Scattering Multipliers");
							ImGui::SliderFloat("Rayleigh Strength", &new_params.rayleigh_multiplier, 0.0f, 10.0f);
							ImGui::SliderFloat("Mie Strength", &new_params.mie_multiplier, 0.0f, 10.0f);
							ImGui::SliderFloat("Sun Intensity", &new_params.sun_intensity, 0.0f, 100.0f);

							ImGui::Separator();
							ImGui::Text("Advanced Physical Parameters");
							if (ImGui::CollapsingHeader("Rayleigh Details")) {
								ImGui::DragFloat3(
									"Base Coefficients",
									&new_params.rayleigh_scattering[0],
									0.0001f,
									0.0f,
									0.1f,
									"%.5f"
								);
								ImGui::SliderFloat("Scale Height", &new_params.rayleigh_scale_height, 0.1f, 50.0f);
							}
							if (ImGui::CollapsingHeader("Mie Details")) {
								ImGui::DragFloat(
									"Base Scattering",
									&new_params.mie_scattering,
									0.0001f,
									0.0f,
									0.1f,
									"%.5f"
								);
								ImGui::DragFloat(
									"Base Extinction",
									&new_params.mie_extinction,
									0.0001f,
									0.0f,
									0.1f,
									"%.5f"
								);
								ImGui::SliderFloat("Anisotropy (g)", &new_params.mie_anisotropy, 0.0f, 0.999f);
								ImGui::SliderFloat("Mie Scale Height", &new_params.mie_scale_height, 0.1f, 20.0f);
							}
							if (ImGui::CollapsingHeader("Atmosphere Geometry")) {
								ImGui::DragFloat("Planet Radius", &new_params.bottom_radius, 1.0f, 100.0f, 10000.0f);
								ImGui::DragFloat("Atmosphere Top", &new_params.top_radius, 1.0f, 100.0f, 10000.0f);
								ImGui::ColorEdit3("Ground Albedo", &new_params.ground_albedo[0]);
							}

							atmosphere_effect->GetScattering().Update(new_params);
						}

						if (ImGui::CollapsingHeader("Cloud Parameters")) {
							float cloud_density = atmosphere_effect->GetCloudDensity();
							if (ImGui::SliderFloat("Cloud Density", &cloud_density, 0.0f, 5.0f)) {
								atmosphere_effect->SetCloudDensity(cloud_density);
							}
							float cloud_altitude = atmosphere_effect->GetCloudAltitude();
							if (ImGui::SliderFloat("Cloud Altitude", &cloud_altitude, -50.0f, 200.0f)) {
								atmosphere_effect->SetCloudAltitude(cloud_altitude);
							}
							float cloud_thickness = atmosphere_effect->GetCloudThickness();
							if (ImGui::SliderFloat("Cloud Thickness", &cloud_thickness, 0.0f, 50.0f)) {
								atmosphere_effect->SetCloudThickness(cloud_thickness);
							}
							glm::vec3 cloud_color = atmosphere_effect->GetCloudColor();
							if (ImGui::ColorEdit3("Cloud Color", &cloud_color[0])) {
								atmosphere_effect->SetCloudColor(cloud_color);
							}
						}

						// Haze params for backward compatibility if needed, but mostly replaced by scattering
						if (ImGui::CollapsingHeader("Legacy Haze (Experimental)")) {
							float haze_density = atmosphere_effect->GetHazeDensity();
							if (ImGui::SliderFloat("Haze Density", &haze_density, 0.0f, 0.5f, "%.4f")) {
								atmosphere_effect->SetHazeDensity(haze_density);
							}
							float haze_height = atmosphere_effect->GetHazeHeight();
							if (ImGui::SliderFloat("Haze Height", &haze_height, 0.0f, 500.0f)) {
								atmosphere_effect->SetHazeHeight(haze_height);
							}
							glm::vec3 haze_color = atmosphere_effect->GetHazeColor();
							if (ImGui::ColorEdit3("Haze Color", &haze_color[0])) {
								atmosphere_effect->SetHazeColor(haze_color);
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
