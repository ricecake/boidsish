#include "ui/EnvironmentWidget.h"

#include "ConfigManager.h"
#include "decor_manager.h"
#include "graphics.h"
#include "imgui.h"
#include "post_processing/PostProcessingManager.h"
#include "post_processing/effects/AtmosphereEffect.h"
#include "terrain_generator_interface.h"

namespace Boidsish {
	namespace UI {

		EnvironmentWidget::EnvironmentWidget(Visualizer& visualizer): m_visualizer(visualizer) {}

		void EnvironmentWidget::Draw() {
			if (!m_show)
				return;

			ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("Environment", &m_show)) {
				// 1. Day/Night Cycle (from LightsWidget)
				if (ImGui::CollapsingHeader("Day/Night Cycle", ImGuiTreeNodeFlags_DefaultOpen)) {
					auto& cycle = m_visualizer.GetLightManager().GetDayNightCycle();
					ImGui::Checkbox("Enabled", &cycle.enabled);
					ImGui::SliderFloat("Time (24h)", &cycle.time, 0.0f, 24.0f, "%.1f h");
					ImGui::SliderFloat("Speed", &cycle.speed, 0.0f, 2.0f, "%.2f");
					ImGui::Checkbox("Paused", &cycle.paused);
				}

				// 2. Atmosphere (from PostProcessingWidget)
				if (ImGui::CollapsingHeader("Atmosphere", ImGuiTreeNodeFlags_DefaultOpen)) {
					auto& manager = m_visualizer.GetPostProcessingManager();
					for (auto& effect : manager.GetPreToneMappingEffects()) {
						if (effect->GetName() == "Atmosphere") {
							bool is_enabled = effect->IsEnabled();
							if (ImGui::Checkbox("Enable Atmosphere", &is_enabled)) {
								effect->SetEnabled(is_enabled);
							}

							if (is_enabled) {
								auto atmosphere_effect =
									std::dynamic_pointer_cast<PostProcessing::AtmosphereEffect>(effect);
								if (atmosphere_effect) {
									float haze_density = atmosphere_effect->GetHazeDensity();
									if (ImGui::SliderFloat("Haze Density", &haze_density, 0.0f, 0.05f, "%.4f")) {
										atmosphere_effect->SetHazeDensity(haze_density);
									}
									float haze_height = atmosphere_effect->GetHazeHeight();
									if (ImGui::SliderFloat("Haze Height", &haze_height, 0.0f, 100.0f)) {
										atmosphere_effect->SetHazeHeight(haze_height);
									}
									glm::vec3 haze_color = atmosphere_effect->GetHazeColor();
									if (ImGui::ColorEdit3("Haze Color", &haze_color[0])) {
										atmosphere_effect->SetHazeColor(haze_color);
									}
									float cloud_density = atmosphere_effect->GetCloudDensity();
									if (ImGui::SliderFloat("Cloud Density", &cloud_density, 0.0f, 1.0f)) {
										atmosphere_effect->SetCloudDensity(cloud_density);
									}
									float cloud_altitude = atmosphere_effect->GetCloudAltitude();
									if (ImGui::SliderFloat("Cloud Altitude", &cloud_altitude, 0.0f, 200.0f)) {
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

									ImGui::Separator();
									ImGui::Text("Scattering");
									float rayleigh = atmosphere_effect->GetRayleighScale();
									if (ImGui::SliderFloat("Rayleigh Scale", &rayleigh, 0.0f, 10.0f)) {
										atmosphere_effect->SetRayleighScale(rayleigh);
									}
									float mie = atmosphere_effect->GetMieScale();
									if (ImGui::SliderFloat("Mie Scale", &mie, 0.0f, 10.0f)) {
										atmosphere_effect->SetMieScale(mie);
									}
									float mie_g = atmosphere_effect->GetMieAnisotropy();
									if (ImGui::SliderFloat("Mie Anisotropy", &mie_g, 0.0f, 0.99f)) {
										atmosphere_effect->SetMieAnisotropy(mie_g);
									}
									float multi_scat = atmosphere_effect->GetMultiScatScale();
									if (ImGui::SliderFloat("MultiScat Scale", &multi_scat, 0.0f, 2.0f)) {
										atmosphere_effect->SetMultiScatScale(multi_scat);
									}
									float ambient_scat = atmosphere_effect->GetAmbientScatScale();
									if (ImGui::SliderFloat("Ambient Scat Scale", &ambient_scat, 0.0f, 1.0f)) {
										atmosphere_effect->SetAmbientScatScale(ambient_scat);
									}
								}
							}
							break;
						}
					}
				}

				// 3. Terrain & Foliage (from ConfigWidget)
				if (ImGui::CollapsingHeader("Terrain", ImGuiTreeNodeFlags_DefaultOpen)) {
					auto terrain = m_visualizer.GetTerrain();
					if (terrain) {
						float world_scale = terrain->GetWorldScale();
						if (ImGui::SliderFloat("World Scale", &world_scale, 0.1f, 5.0f)) {
							terrain->SetWorldScale(world_scale);
						}
						ImGui::Text("Higher = larger world, Lower = smaller world");
					}

					auto decor_manager = m_visualizer.GetDecorManager();
					if (decor_manager) {
						bool enabled = decor_manager->IsEnabled();
						if (ImGui::Checkbox("Enable Foliage", &enabled)) {
							decor_manager->SetEnabled(enabled);
						}
					}
				}

				// 4. Wind (from EffectsWidget)
				if (ImGui::CollapsingHeader("Wind", ImGuiTreeNodeFlags_DefaultOpen)) {
					auto& config = ConfigManager::GetInstance();

					float wind_strength = config.GetAppSettingFloat("wind_strength", 0.065f);
					if (ImGui::SliderFloat("Wind Strength", &wind_strength, 0.0f, 5.0f)) {
						config.SetFloat("wind_strength", wind_strength);
					}

					float wind_speed = config.GetAppSettingFloat("wind_speed", 0.075f);
					if (ImGui::SliderFloat("Wind Speed", &wind_speed, 0.0f, 10.0f)) {
						config.SetFloat("wind_speed", wind_speed);
					}

					float wind_frequency = config.GetAppSettingFloat("wind_frequency", 0.01f);
					if (ImGui::SliderFloat("Wind Frequency", &wind_frequency, 0.01f, 1.0f)) {
						config.SetFloat("wind_frequency", wind_frequency);
					}
				}
			}
			ImGui::End();
		}
	} // namespace UI
} // namespace Boidsish
