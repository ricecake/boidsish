#include "ui/EnvironmentWidget.h"

#include "ConfigManager.h"
#include "decor_manager.h"
#include "graphics.h"
#include "imgui.h"
#include "post_processing/PostProcessingManager.h"
#include "post_processing/effects/AtmosphereEffect.h"
#include "terrain_generator_interface.h"
#include "weather_manager.h"

namespace Boidsish {
	namespace UI {

		EnvironmentWidget::EnvironmentWidget(Visualizer& visualizer): m_visualizer(visualizer) {}

		void EnvironmentWidget::Draw() {
			if (!m_show)
				return;

			ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("Environment", &m_show)) {
				// 0. Weather
				if (ImGui::CollapsingHeader("Ambient Weather", ImGuiTreeNodeFlags_DefaultOpen)) {
					auto weather = m_visualizer.GetWeatherManager();
					if (weather) {
						bool enabled = weather->IsEnabled();
						if (ImGui::Checkbox("Enable Weather System", &enabled)) {
							weather->SetEnabled(enabled);
						}

						if (enabled) {
							float time_scale = weather->GetTimeScale();
							if (ImGui::SliderFloat("Weather Time Scale", &time_scale, 0.0f, 0.015f, "%.3f")) {
								weather->SetTimeScale(time_scale);
							}

							float spatial_scale = weather->GetSpatialScale();
							if (ImGui::SliderFloat("Weather Spatial Scale", &spatial_scale, 0.0f, 0.003f, "%.4f")) {
								weather->SetSpatialScale(spatial_scale);
							}

							float hold_threshold = weather->GetHoldThreshold();
							if (ImGui::SliderFloat("Hold Threshold", &hold_threshold, 0.0f, 1.0f, "%.2f")) {
								weather->SetHoldThreshold(hold_threshold);
							}

							ImGui::Separator();

							// Weather selection dropdown
							int                      manual_idx = weather->GetManualPreset();
							int                      current_item = manual_idx + 1; // 0 = Dynamic, 1+ = Presets
							std::vector<std::string> preset_names = weather->GetPresetNames();
							std::vector<const char*> items;
							items.push_back("Dynamic");
							for (const auto& name : preset_names) {
								items.push_back(name.c_str());
							}

							if (ImGui::Combo("Weather Type", &current_item, items.data(), (int)items.size())) {
								weather->SetManualPreset(current_item - 1);
							}

							const auto& blend = weather->GetBlendingInfo();
							if (blend.is_manual) {
								ImGui::Text("Current Weather: %s (Manual)", blend.low_name.c_str());
							} else {
								if (blend.low_name == blend.high_name) {
									ImGui::Text("Current Weather: %s", blend.low_name.c_str());
								} else {
									ImGui::Text(
										"Current Weather: %s / %s (%.0f%%)",
										blend.low_name.c_str(),
										blend.high_name.c_str(),
										blend.t * 100.0f
									);
								}
							}
						}
					}
				}

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
								auto atmosphere_effect = std::dynamic_pointer_cast<PostProcessing::AtmosphereEffect>(
									effect
								);
								if (atmosphere_effect) {
									float haze_density = atmosphere_effect->GetHazeDensity();
									if (ImGui::SliderFloat("Haze Density", &haze_density, 0.0f, 5.0f, "%.2f")) {
										atmosphere_effect->SetHazeDensity(haze_density);
									}
									float haze_height = atmosphere_effect->GetHazeHeight();
									if (ImGui::SliderFloat("Haze Height", &haze_height, 0.0f, 50.0f)) {
										atmosphere_effect->SetHazeHeight(haze_height);
									}
									glm::vec3 haze_color = atmosphere_effect->GetHazeColor();
									if (ImGui::ColorEdit3("Haze Color", &haze_color[0])) {
										atmosphere_effect->SetHazeColor(haze_color);
									}
									float cloud_density = atmosphere_effect->GetCloudDensity();
									if (ImGui::SliderFloat("Cloud Density", &cloud_density, 0.0f, 50.0f)) {
										atmosphere_effect->SetCloudDensity(cloud_density);
									}
									float cloud_altitude = atmosphere_effect->GetCloudAltitude();
									if (ImGui::SliderFloat("Cloud Altitude", &cloud_altitude, 0.0f, 1000.0f)) {
										atmosphere_effect->SetCloudAltitude(cloud_altitude);
									}
									float cloud_thickness = atmosphere_effect->GetCloudThickness();
									if (ImGui::SliderFloat("Cloud Thickness", &cloud_thickness, 0.0f, 500.0f)) {
										atmosphere_effect->SetCloudThickness(cloud_thickness);
									}
									float cloud_coverage = atmosphere_effect->GetCloudCoverage();
									if (ImGui::SliderFloat("Cloud Coverage", &cloud_coverage, 0.0f, 2.0f)) {
										atmosphere_effect->SetCloudCoverage(cloud_coverage);
									}
									float cloud_warp = atmosphere_effect->GetCloudWarp();
									if (ImGui::SliderFloat("Camera Cloud Buffer", &cloud_warp, 0.0f, 1000.0f)) {
										atmosphere_effect->SetCloudWarp(cloud_warp);
									}
									glm::vec3 cloud_color = atmosphere_effect->GetCloudColor();
									if (ImGui::ColorEdit3("Cloud Color", &cloud_color[0])) {
										atmosphere_effect->SetCloudColor(cloud_color);
									}

									auto& cfg = ConfigManager::GetInstance();

									float cloud_shadow = cfg.GetAppSettingFloat("cloud_shadow_intensity", 0.5f);
									if (ImGui::SliderFloat("Cloud Shadow Intensity", &cloud_shadow, 0.0f, 1.0f)) {
										cfg.SetFloat("cloud_shadow_intensity", cloud_shadow);
									}

									ImGui::Separator();
									ImGui::Text("Advanced Cloud Parameters");

									float g1 = cfg.GetAppSettingFloat("cloud_phase_g1", atmosphere_effect->GetCloudPhaseG1());
									if (ImGui::SliderFloat("Phase G1 (Forward)", &g1, 0.0f, 0.99f)) {
										atmosphere_effect->SetCloudPhaseG1(g1);
										cfg.SetFloat("cloud_phase_g1", g1);
									}
									float g2 = cfg.GetAppSettingFloat("cloud_phase_g2", atmosphere_effect->GetCloudPhaseG2());
									if (ImGui::SliderFloat("Phase G2 (Backward)", &g2, -0.99f, 0.0f)) {
										atmosphere_effect->SetCloudPhaseG2(g2);
										cfg.SetFloat("cloud_phase_g2", g2);
									}
									float alpha = cfg.GetAppSettingFloat("cloud_phase_alpha", atmosphere_effect->GetCloudPhaseAlpha());
									if (ImGui::SliderFloat("Phase Mix Alpha", &alpha, 0.0f, 1.0f)) {
										atmosphere_effect->SetCloudPhaseAlpha(alpha);
										cfg.SetFloat("cloud_phase_alpha", alpha);
									}
									float isotropic = cfg.GetAppSettingFloat("cloud_phase_isotropic", atmosphere_effect->GetCloudPhaseIsotropic());
									if (ImGui::SliderFloat("Phase Isotropic Mix", &isotropic, 0.0f, 1.0f)) {
										atmosphere_effect->SetCloudPhaseIsotropic(isotropic);
										cfg.SetFloat("cloud_phase_isotropic", isotropic);
									}

									float p_scale = cfg.GetAppSettingFloat("cloud_powder_scale", atmosphere_effect->GetCloudPowderScale());
									if (ImGui::SliderFloat("Powder Scale", &p_scale, 0.0f, 2.0f)) {
										atmosphere_effect->SetCloudPowderScale(p_scale);
										cfg.SetFloat("cloud_powder_scale", p_scale);
									}
									float p_mult = cfg.GetAppSettingFloat("cloud_powder_multiplier", atmosphere_effect->GetCloudPowderMultiplier());
									if (ImGui::SliderFloat("Powder Multiplier", &p_mult, 0.0f, 2.0f)) {
										atmosphere_effect->SetCloudPowderMultiplier(p_mult);
										cfg.SetFloat("cloud_powder_multiplier", p_mult);
									}
									float p_local = cfg.GetAppSettingFloat("cloud_powder_local_scale", atmosphere_effect->GetCloudPowderLocalScale());
									if (ImGui::SliderFloat("Powder Local Scale", &p_local, 0.0f, 10.0f)) {
										atmosphere_effect->SetCloudPowderLocalScale(p_local);
										cfg.SetFloat("cloud_powder_local_scale", p_local);
									}

									float s_opt = cfg.GetAppSettingFloat("cloud_shadow_optical_depth_multiplier", atmosphere_effect->GetCloudShadowOpticalDepthMultiplier());
									if (ImGui::SliderFloat("Shadow Optical Depth Mult", &s_opt, 0.0f, 1.0f)) {
										atmosphere_effect->SetCloudShadowOpticalDepthMultiplier(s_opt);
										cfg.SetFloat("cloud_shadow_optical_depth_multiplier", s_opt);
									}
									float s_step = cfg.GetAppSettingFloat("cloud_shadow_step_multiplier", atmosphere_effect->GetCloudShadowStepMultiplier());
									if (ImGui::SliderFloat("Shadow Step Mult", &s_step, 0.0f, 1.0f)) {
										atmosphere_effect->SetCloudShadowStepMultiplier(s_step);
										cfg.SetFloat("cloud_shadow_step_multiplier", s_step);
									}

									float sun_scale = cfg.GetAppSettingFloat("cloud_sun_light_scale", atmosphere_effect->GetCloudSunLightScale());
									if (ImGui::SliderFloat("Sun Light Scale", &sun_scale, 0.0f, 50.0f)) {
										atmosphere_effect->SetCloudSunLightScale(sun_scale);
										cfg.SetFloat("cloud_sun_light_scale", sun_scale);
									}
									float moon_scale = cfg.GetAppSettingFloat("cloud_moon_light_scale", atmosphere_effect->GetCloudMoonLightScale());
									if (ImGui::SliderFloat("Moon Light Scale", &moon_scale, 0.0f, 20.0f)) {
										atmosphere_effect->SetCloudMoonLightScale(moon_scale);
										cfg.SetFloat("cloud_moon_light_scale", moon_scale);
									}

									float bp_mix = cfg.GetAppSettingFloat("cloud_beer_powder_mix", atmosphere_effect->GetCloudBeerPowderMix());
									if (ImGui::SliderFloat("Beer-Powder Mix", &bp_mix, 0.0f, 1.0f)) {
										atmosphere_effect->SetCloudBeerPowderMix(bp_mix);
										cfg.SetFloat("cloud_beer_powder_mix", bp_mix);
									}

									ImGui::Separator();
									ImGui::Text("Scattering");
									float rayleigh = atmosphere_effect->GetRayleighScale();
									if (ImGui::SliderFloat("Rayleigh Scale", &rayleigh, 0.0f, 3.0f)) {
										atmosphere_effect->SetRayleighScale(rayleigh);
									}
									float mie = atmosphere_effect->GetMieScale();
									if (ImGui::SliderFloat("Mie Scale", &mie, 0.0f, 0.25f)) {
										atmosphere_effect->SetMieScale(mie);
									}
									float mie_g = atmosphere_effect->GetMieAnisotropy();
									if (ImGui::SliderFloat("Mie Anisotropy", &mie_g, 0.0f, 0.99f)) {
										atmosphere_effect->SetMieAnisotropy(mie_g);
									}
									float multi_scat = atmosphere_effect->GetMultiScatScale();
									if (ImGui::SliderFloat("MultiScat Scale", &multi_scat, 0.0f, 0.25f)) {
										atmosphere_effect->SetMultiScatScale(multi_scat);
									}
									float ambient_scat = atmosphere_effect->GetAmbientScatScale();
									if (ImGui::SliderFloat("Ambient Scat Scale", &ambient_scat, 0.0f, 2.0f)) {
										atmosphere_effect->SetAmbientScatScale(ambient_scat);
									}

									ImGui::Separator();
									ImGui::Text("Physical Parameters");

									float atmos_height = atmosphere_effect->GetAtmosphereHeight();
									if (ImGui::SliderFloat("Atmosphere Height (km)", &atmos_height, 0.0f, 300.0f)) {
										atmosphere_effect->SetAtmosphereHeight(atmos_height);
									}

									glm::vec3 rayleigh_scattering = atmosphere_effect->GetRayleighScattering() *
										1000.0f;
									if (ImGui::ColorEdit3("Rayleigh Scattering", &rayleigh_scattering[0])) {
										atmosphere_effect->SetRayleighScattering(rayleigh_scattering * 0.001f);
									}

									float mie_scat = atmosphere_effect->GetMieScattering() * 1000.0f;
									if (ImGui::SliderFloat("Mie Scattering coeff", &mie_scat, 0.0f, 10.0f)) {
										atmosphere_effect->SetMieScattering(mie_scat * 0.001f);
									}

									float mie_ext = atmosphere_effect->GetMieExtinction() * 1000.0f;
									if (ImGui::SliderFloat("Mie Extinction coeff", &mie_ext, 0.0f, 10.0f)) {
										atmosphere_effect->SetMieExtinction(mie_ext * 0.001f);
									}

									glm::vec3 ozone_absorption = atmosphere_effect->GetOzoneAbsorption() * 1000.0f;
									if (ImGui::ColorEdit3("Ozone Absorption", &ozone_absorption[0])) {
										atmosphere_effect->SetOzoneAbsorption(ozone_absorption * 0.001f);
									}

									float rayleigh_h = atmosphere_effect->GetRayleighScaleHeight();
									if (ImGui::SliderFloat("Rayleigh Scale Height (km)", &rayleigh_h, 0.0f, 20.0f)) {
										atmosphere_effect->SetRayleighScaleHeight(rayleigh_h);
									}

									float mie_h = atmosphere_effect->GetMieScaleHeight();
									if (ImGui::SliderFloat("Mie Scale Height (km)", &mie_h, 0.0f, 3.0f)) {
										atmosphere_effect->SetMieScaleHeight(mie_h);
									}

									ImGui::Separator();
									ImGui::Text("Atmosphere Variance");

									float var_scale = atmosphere_effect->GetColorVarianceScale();
									if (ImGui::SliderFloat("Variance Scale", &var_scale, 0.0f, 2.5f)) {
										atmosphere_effect->SetColorVarianceScale(var_scale);
									}

									float var_strength = atmosphere_effect->GetColorVarianceStrength();
									if (ImGui::SliderFloat("Variance Strength", &var_strength, 0.0f, 0.5f)) {
										atmosphere_effect->SetColorVarianceStrength(var_strength);
									}
								}
							}
							break;
						}
					}
				}

				// 3. Terrain & Foliage (from ConfigWidget)
				if (ImGui::CollapsingHeader("Terrain", ImGuiTreeNodeFlags_DefaultOpen)) {
					auto& config = ConfigManager::GetInstance();
					bool  render_terrain = config.GetAppSettingBool("render_terrain", true);
					if (ImGui::Checkbox("Render Terrain", &render_terrain)) {
						config.SetBool("render_terrain", render_terrain);
					}
					bool render_floor = config.GetAppSettingBool("render_floor", true);
					if (ImGui::Checkbox("Render Floor", &render_floor)) {
						config.SetBool("render_floor", render_floor);
					}
					bool force_both = config.GetAppSettingBool("force_both_floor_and_terrain", false);
					if (ImGui::Checkbox("Force Both Floor and Terrain", &force_both)) {
						config.SetBool("force_both_floor_and_terrain", force_both);
					}

					ImGui::Separator();

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

						if (enabled) {
							float threshold = ConfigManager::GetInstance().GetAppSettingFloat(
								"foliage_culling_pixel_threshold",
								10.0f
							);
							if (ImGui::SliderFloat("Foliage Pixel Threshold", &threshold, 0.0f, 50.0f)) {
								ConfigManager::GetInstance().SetFloat("foliage_culling_pixel_threshold", threshold);
							}
							ImGui::Text("Higher = cull larger objects, 0 = disable size culling");
						}
					}
				}

				// 4. Particles
				if (ImGui::CollapsingHeader("Particles", ImGuiTreeNodeFlags_DefaultOpen)) {
					auto& config = ConfigManager::GetInstance();
					float density = config.GetAppSettingFloat("ambient_particle_density", 0.15f);
					if (ImGui::SliderFloat("Ambient Density", &density, 0.0f, 1.0f)) {
						config.SetFloat("ambient_particle_density", density);
					}
					ImGui::Text("Controls the spawn rate of ambient particles.");
				}

				// 5. Wind (from EffectsWidget)
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

				// 6. Terrain Erosion
				if (ImGui::CollapsingHeader("Terrain Erosion", ImGuiTreeNodeFlags_DefaultOpen)) {
					auto& config = ConfigManager::GetInstance();

					bool erosion_enabled = config.GetAppSettingBool("erosion_enabled", true);
					if (ImGui::Checkbox("Enable Erosion Filter", &erosion_enabled)) {
						config.SetBool("erosion_enabled", erosion_enabled);
					}

					if (erosion_enabled) {
						float erosion_strength = config.GetAppSettingFloat("erosion_strength", 0.12f);
						if (ImGui::SliderFloat("Erosion Strength", &erosion_strength, 0.0f, 0.5f, "%.3f")) {
							config.SetFloat("erosion_strength", erosion_strength);
						}

						float erosion_scale = config.GetAppSettingFloat("erosion_scale", 0.15f);
						if (ImGui::SliderFloat("Erosion Scale", &erosion_scale, 0.01f, 1.0f, "%.2f")) {
							config.SetFloat("erosion_scale", erosion_scale);
						}

						float erosion_detail = config.GetAppSettingFloat("erosion_detail", 1.5f);
						if (ImGui::SliderFloat("Erosion Detail", &erosion_detail, 0.1f, 5.0f, "%.1f")) {
							config.SetFloat("erosion_detail", erosion_detail);
						}

						float erosion_gully_weight = config.GetAppSettingFloat("erosion_gully_weight", 0.5f);
						if (ImGui::SliderFloat("Gully Weight", &erosion_gully_weight, 0.0f, 1.0f, "%.2f")) {
							config.SetFloat("erosion_gully_weight", erosion_gully_weight);
						}

						float erosion_max_dist = config.GetAppSettingFloat("erosion_max_dist", 450.0f);
						if (ImGui::SliderFloat("Erosion Max Dist", &erosion_max_dist, 50.0f, 1000.0f, "%.0f")) {
							config.SetFloat("erosion_max_dist", erosion_max_dist);
						}
					}
				}
			}
			ImGui::End();
		}
	} // namespace UI
} // namespace Boidsish
