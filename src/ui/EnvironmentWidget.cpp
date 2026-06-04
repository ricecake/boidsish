#include "ui/EnvironmentWidget.h"

#include <cmath>

#include "ConfigManager.h"
#include "constants.h"
#include "decor_manager.h"
#include "fire_effect_manager.h"
#include "grass_manager.h"
#include "graphics.h"
#include "imgui.h"
#include "post_processing/PostProcessingManager.h"
#include "post_processing/effects/AtmosphereEffect.h"
#include "post_processing/effects/VolumetricLightingEffect.h"
#include "terrain_generator_interface.h"
#include "weather_manager.h"
#include "state.h"
#include "state_frame.h"
#include "service_locator.h"
#include "ui/Widgets.h"

namespace Boidsish {
	namespace UI {

		EnvironmentWidget::EnvironmentWidget(Visualizer& visualizer): m_visualizer(visualizer) {}

		void EnvironmentWidget::Draw() {
			if (!m_show)
				return;

			auto fb = ServiceLocator::Instance().Get<state::FrameBuffer>();
			const auto& input = *fb->Read().user_input;
			const auto& sim = fb->Read().simulation;
			const auto& eff = *fb->Read().effective;

			ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("Environment", &m_show)) {
				auto weather = m_visualizer.GetWeatherManager();

				// Helper for weather attribute sliders with T/min/max/U constraint buttons.
				// The slider uses SliderFloatWithActual to show user input vs simulation actual.
				// Constraint buttons still use weather manager directly (transient constraint management).
				auto drawAttrControl = [&](const char* label, const char* constraint_key, float user_val, float actual_val, float min_v, float max_v, const char* fmt, auto action_factory) {
					float val = user_val;

					ImGui::Text("%s:", label);
					if (SliderFloatWithActual((std::string("##slide") + label).c_str(), &val, actual_val, min_v, max_v)) {
						fb->Apply(action_factory(val));
					}
					ImGui::SameLine();
					if (ImGui::Button((std::string("T##") + label).c_str())) {
						state::AttributeConstraint c;
						c.target = val;
						fb->Apply(state::actions::SetWeatherAttrConstraint{constraint_key, c});
					}
					ImGui::SetItemTooltip("Set Target");
					ImGui::SameLine();
					if (ImGui::Button((std::string("min##") + label).c_str())) {
						state::AttributeConstraint c;
						c.min = val;
						fb->Apply(state::actions::SetWeatherAttrConstraint{constraint_key, c});
					}
					ImGui::SetItemTooltip("Set Minimum");
					ImGui::SameLine();
					if (ImGui::Button((std::string("max##") + label).c_str())) {
						state::AttributeConstraint c;
						c.max = val;
						fb->Apply(state::actions::SetWeatherAttrConstraint{constraint_key, c});
					}
					ImGui::SetItemTooltip("Set Maximum");
					ImGui::SameLine();
					if (ImGui::Button((std::string("U##") + label).c_str())) {
						fb->Apply(state::actions::ClearWeatherAttrConstraint{constraint_key});
					}
					ImGui::SetItemTooltip("Unlock / Clear Constraints");
				};

				// 0. Weather
				if (ImGui::CollapsingHeader("Ambient Weather", ImGuiTreeNodeFlags_DefaultOpen)) {
					if (weather) {
						bool enabled = input.weather.enabled;
						if (ImGui::Checkbox("Enable Weather System", &enabled)) {
							fb->Apply(state::actions::SetWeatherEnabled{enabled});
						}

						if (enabled) {
							float time_scale = input.weather.timeScale;
							if (ImGui::SliderFloat("Weather Time Scale", &time_scale, 0.0f, 0.015f, "%.3f")) {
								fb->Apply(state::actions::SetWeatherTimeScale{time_scale});
							}

							float spatial_scale = input.weather.spatialScale;
							if (ImGui::SliderFloat("Weather Spatial Scale", &spatial_scale, 0.0f, 0.003f, "%.4f")) {
								fb->Apply(state::actions::SetWeatherSpatialScale{spatial_scale});
							}

							float hold_threshold = input.weather.holdThreshold;
							if (ImGui::SliderFloat("Hold Threshold", &hold_threshold, 0.0f, 1.0f, "%.2f")) {
								fb->Apply(state::actions::SetWeatherHoldThreshold{hold_threshold});
							}

							if (ImGui::Button("Clear All Weather Constraints")) {
								fb->Apply(state::actions::ClearAllWeatherAttrConstraints{});
							}

							ImGui::Separator();

							drawAttrControl("Temperature (K)", "temperature", input.weather.temperature, sim.weather.temperature, 200.0f, 350.0f, "%.1f K", [](float v){ return state::actions::SetWeatherTemperature{v}; });
							float temp = sim.weather.temperature;
							ImGui::Text("  (%.1f C / %.1f F)", temp - 273.15f, (temp - 273.15f) * 1.8f + 32.0f);

							drawAttrControl("Precipitation", "precipitation", input.weather.precipitation, sim.weather.precipitation, 0.0f, 1.0f, "%.2f", [](float v){ return state::actions::SetWeatherPrecipitation{v}; });
							drawAttrControl("Humidity", "humidity", input.weather.humidity, sim.weather.humidity, 0.0f, 1.0f, "%.2f", [](float v){ return state::actions::SetWeatherHumidity{v}; });
							drawAttrControl("Wind Strength", "windStrength", input.weather.windStrength, sim.weather.windStrength, 0.0f, 5.0f, "%.2f", [](float v){ return state::actions::SetWeatherWindStrength{v}; });
							drawAttrControl("Cloud Coverage", "cloudCoverage", input.weather.cloudCoverage, sim.weather.cloudCoverage, 0.0f, 1.0f, "%.2f", [](float v){ return state::actions::SetWeatherCloudCoverage{v}; });

							auto active_constraints = weather->GetActiveConstraints();
							if (!active_constraints.empty()) {
								ImGui::Separator();
								ImGui::Text("Active Weather Constraints:");
								for (const auto& con : active_constraints) {
									std::string text = WeatherManager::GetAttributeName(con.attr) + ": ";
									if (con.target) text += "Target=" + std::to_string(*con.target).substr(0, 5) + " ";
									if (con.min) text += "Min=" + std::to_string(*con.min).substr(0, 5) + " ";
									if (con.max) text += "Max=" + std::to_string(*con.max).substr(0, 5) + " ";

									ImGui::Text("%s", text.c_str());
									ImGui::SameLine();
									if (ImGui::Button((std::string("X##") + text).c_str())) {
										weather->ClearTarget(con.attr);
										weather->ClearMin(con.attr);
										weather->ClearMax(con.attr);
									}
								}
							}

							// Display derived intensities and wetness
							float rain = (sim.weather.temperature > 273.15f) ? sim.weather.precipitation : 0.0f;
							float snow = (sim.weather.temperature <= 273.15f) ? sim.weather.precipitation : 0.0f;
							ImGui::Text("Rain Intensity: %.2f", rain);
							ImGui::Text("Snow Intensity: %.2f", snow);

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

							ImGui::Separator();
							if (ImGui::TreeNode("Macro Weather Simulation")) {
								bool macro_enabled = input.weather.macroSimEnabled;
								if (ImGui::Checkbox("Enable Macro Simulation", &macro_enabled)) {
									fb->Apply(state::actions::SetWeatherMacroSimEnabled{macro_enabled});
								}

								if (macro_enabled) {
									float tau = input.weather.simTau;
									if (ImGui::SliderFloat("Simulation Relaxation (Tau)", &tau, 0.51f, 2.0f, "%.2f")) {
										fb->Apply(state::actions::SetWeatherSimTau{tau});
									}

									bool strict = input.weather.strictEnforcement;
									if (ImGui::Checkbox("Strict Enforcement (Instant Snap)", &strict)) {
										fb->Apply(state::actions::SetWeatherStrictEnforcement{strict});
									}

									float stiffness = input.weather.nudgeStiffness;
									if (ImGui::SliderFloat("Nudge Stiffness", &stiffness, 0.01f, 10.0f, "%.2f")) {
										fb->Apply(state::actions::SetWeatherNudgeStiffness{stiffness});
									}

									if (ImGui::Button("Reset Simulation")) {
										weather->ResetMacroSim();
									}

									const auto* phys = weather->GetPhysicallyBasedWeather();
									if (phys) {
										ImGui::Text("Simulation State:");
										ImGui::Text(
											"Wind: (%.2f, %.2f) m/s (H), %.2f m/s (V)",
											phys->windVelocity.x,
											phys->windVelocity.y,
											phys->verticalWind
										);
										ImGui::Text("Temp: %.1f K (%.1f C)", phys->temperature, phys->temperature - 273.15f);
										ImGui::Text("Pressure: %.1f hPa", phys->pressure);
										ImGui::Text("Humidity: %.1f%%", phys->humidity * 100.0f);
										ImGui::Text("Cloud Coverage: %.1f%%", phys->cloudCoverage * 100.0f);
									}

									ImGui::Separator();

								if (ImGui::TreeNode("Weather Constraints & Nudges")) {
									auto& constraints = weather->GetSimConstraints();
									bool  changed = false;

									auto drawConstraint = [&](const char* label, const char* prefix, WeatherLbmSimulator::Constraint& con, float min_val, float max_val, const char* format) {
										ImGui::Text("%s:", label);
										ImGui::Indent();

										bool min_enabled = con.min.has_value();
										if (ImGui::Checkbox(("Min##" + std::string(prefix)).c_str(), &min_enabled)) {
											if (min_enabled) con.min = min_val;
											else con.min = std::nullopt;
											changed = true;
										}
										if (min_enabled) {
											ImGui::SameLine();
											float val = *con.min;
											if (ImGui::SliderFloat(("##min_" + std::string(prefix)).c_str(), &val, min_val, max_val, format)) {
												con.min = val;
												changed = true;
											}
										}

										bool max_enabled = con.max.has_value();
										if (ImGui::Checkbox(("Max##" + std::string(prefix)).c_str(), &max_enabled)) {
											if (max_enabled) con.max = max_val;
											else con.max = std::nullopt;
											changed = true;
										}
										if (max_enabled) {
											ImGui::SameLine();
											float val = *con.max;
											if (ImGui::SliderFloat(("##max_" + std::string(prefix)).c_str(), &val, min_val, max_val, format)) {
												con.max = val;
												changed = true;
											}
										}

										bool target_enabled = con.target.has_value();
										if (ImGui::Checkbox(("Target##" + std::string(prefix)).c_str(), &target_enabled)) {
											if (target_enabled) con.target = (min_val + max_val) * 0.5f;
											else con.target = std::nullopt;
											changed = true;
										}
										if (target_enabled) {
											ImGui::SameLine();
											float val = *con.target;
											if (ImGui::SliderFloat(("##target_" + std::string(prefix)).c_str(), &val, min_val, max_val, format)) {
												con.target = val;
												changed = true;
											}
										}

										if (ImGui::Button(("Clear All##" + std::string(prefix)).c_str())) {
											con.min = std::nullopt;
											con.max = std::nullopt;
											con.target = std::nullopt;
											changed = true;
										}

										ImGui::Unindent();
										ImGui::Separator();
									};

									WeatherLbmSimulator::Constraints c = constraints;
									drawConstraint("Temperature (K)", "temp", c.temperature, 200.0f, 400.0f, "%.1f K");
									drawConstraint("Pressure (hPa)", "press", c.pressure, 800.0f, 1200.0f, "%.1f hPa");
									drawConstraint("Humidity", "hum", c.humidity, 0.0f, 1.0f, "%.2f");
									drawConstraint("Wind Velocity (m/s)", "vel", c.velocity, 0.0f, 50.0f, "%.1f m/s");

									if (changed) {
										weather->SetSimConstraints(c);
									}

									ImGui::TreePop();
								}

									if (ImGui::TreeNode("Atmospheric Injections")) {
										ImGui::SliderFloat("Target Pressure (hPa)", &m_targetPressure, 800.0f, 1200.0f, "%.1f");
										ImGui::SliderFloat("Target Temperature (K)", &m_targetTemperature, 200.0f, 400.0f, "%.1f");
										ImGui::SliderFloat("Target Aerosol", &m_targetAerosol, 0.0f, 1.0f, "%.3f");
										ImGui::SliderFloat("Burst Strength", &m_burstStrength, 0.0f, 0.15f, "%.3f");

										auto get_target_pos = [&]() -> glm::vec3 {
											auto screen_w = ImGui::GetIO().DisplaySize.x;
											auto screen_h = ImGui::GetIO().DisplaySize.y;
											auto pos = m_visualizer.ScreenToWorld(screen_w * 0.5f, screen_h * 0.5f);
											return pos.value_or(m_visualizer.GetCamera().pos());
										};

										if (ImGui::Button("Inject Pressure")) {
											fb->Apply(state::actions::InjectPressure{get_target_pos(), m_targetPressure, m_burstStrength});
										}
										ImGui::SameLine();
										if (ImGui::Button("Burst (1100 hPa)")) {
											fb->Apply(state::actions::InjectPressure{get_target_pos(), 1100.0f, 0.1f});
										}
										ImGui::SameLine();
										if (ImGui::Button("Vacuum (900 hPa)")) {
											fb->Apply(state::actions::InjectPressure{get_target_pos(), 900.0f, 0.0f});
										}

										if (ImGui::Button("Inject Temperature")) {
											fb->Apply(state::actions::InjectTemperature{get_target_pos(), m_targetTemperature});
										}
										ImGui::SameLine();
										if (ImGui::Button("Heat (350 K)")) {
											fb->Apply(state::actions::InjectTemperature{get_target_pos(), 350.0f});
										}
										ImGui::SameLine();
										if (ImGui::Button("Cold (250 K)")) {
											fb->Apply(state::actions::InjectTemperature{get_target_pos(), 250.0f});
										}

										if (ImGui::Button("Inject Aerosol")) {
											fb->Apply(state::actions::InjectAerosol{get_target_pos(), m_targetAerosol});
										}
										ImGui::SameLine();
										if (ImGui::Button("Dusty (0.5)")) {
											fb->Apply(state::actions::InjectAerosol{get_target_pos(), 0.5f});
										}
										ImGui::SameLine();
										if (ImGui::Button("Clean (0.0)")) {
											fb->Apply(state::actions::InjectAerosol{get_target_pos(), 0.0f});
										}

										ImGui::TreePop();
									}
								}
								ImGui::TreePop();
							}
						}
					}
				}

				// 3.5 Grass
				if (ImGui::CollapsingHeader("Grass", ImGuiTreeNodeFlags_DefaultOpen)) {
					bool enabled = input.grass.enabled;
					if (ImGui::Checkbox("Enable Grass", &enabled)) {
						fb->Apply(state::actions::SetGrassEnabled{enabled});
					}

					if (enabled) {
						float length = input.grass.lengthMultiplier;
						if (ImGui::SliderFloat("Length Multiplier", &length, 0.1f, 5.0f)) {
							fb->Apply(state::actions::SetGrassLength{length});
						}
						float width = input.grass.widthMultiplier;
						if (ImGui::SliderFloat("Width Multiplier", &width, 0.1f, 5.0f)) {
							fb->Apply(state::actions::SetGrassWidth{width});
						}
						float density = input.grass.densityMultiplier;
						if (ImGui::SliderFloat("Density Multiplier", &density, 0.0f, 2.0f)) {
							fb->Apply(state::actions::SetGrassDensity{density});
						}
						float rigidity = input.grass.rigidityMultiplier;
						if (ImGui::SliderFloat("Rigidity Multiplier", &rigidity, 0.0f, 2.0f)) {
							fb->Apply(state::actions::SetGrassRigidity{rigidity});
						}
						float wind = input.grass.windMultiplier;
						if (ImGui::SliderFloat("Wind Multiplier", &wind, 0.0f, 5.0f)) {
							fb->Apply(state::actions::SetGrassWind{wind});
						}

						ImGui::Separator();
						ImGui::Text("Procedural Scaling");
						float lodScaleFactor = input.grass.lodScaleFactor;
						if (ImGui::SliderFloat("LOD Scale Factor", &lodScaleFactor, 1.1f, 4.0f)) {
							fb->Apply(state::actions::SetGrassLodScaleFactor{lodScaleFactor});
						}
						float lodBaseRange = input.grass.lodBaseRange;
						if (ImGui::SliderFloat("LOD Base Range", &lodBaseRange, 5.0f, 100.0f)) {
							fb->Apply(state::actions::SetGrassLodBaseRange{lodBaseRange});
						}
						float baseScale = input.grass.baseScale;
						if (ImGui::SliderFloat("Base Grid Scale", &baseScale, 0.05f, 2.0f)) {
							fb->Apply(state::actions::SetGrassBaseScale{baseScale});
						}
					}
				}

				// 1. Day/Night Cycle
				if (ImGui::CollapsingHeader("Day/Night Cycle", ImGuiTreeNodeFlags_DefaultOpen)) {
					bool dn_enabled = input.dayNight.enabled;
					if (ImGui::Checkbox("Enabled", &dn_enabled)) {
						fb->Apply(state::actions::SetDayNightEnabled{dn_enabled});
					}
					float dn_time = input.dayNight.time;
					if (ImGui::SliderFloat("Time (24h)", &dn_time, 0.0f, 24.0f, "%.1f h")) {
						fb->Apply(state::actions::SetDayNightTime{dn_time});
					}
					float dn_speed = input.dayNight.speed;
					if (ImGui::SliderFloat("Speed", &dn_speed, 0.0f, 2.0f, "%.2f")) {
						fb->Apply(state::actions::SetDayNightSpeed{dn_speed});
					}
					bool dn_paused = input.dayNight.paused;
					if (ImGui::Checkbox("Paused", &dn_paused)) {
						fb->Apply(state::actions::SetDayNightPaused{dn_paused});
					}

					ImGui::Separator();
					ImGui::Text("Moon");
					float albedo = input.dayNight.lunarAlbedo;
					if (ImGui::SliderFloat("Lunar Albedo", &albedo, 0.0f, 0.5f, "%.3f")) {
						fb->Apply(state::actions::SetLunarAlbedo{albedo});
					}
					glm::vec3 tint = input.dayNight.moonTint;
					if (ImGui::ColorEdit3("Moon Tint", &tint[0])) {
						fb->Apply(state::actions::SetMoonTint{tint});
					}
					float month = input.dayNight.lunarMonth;
					if (ImGui::SliderFloat("Lunar Month (days)", &month, 0.1f, 30.0f, "%.2f")) {
						fb->Apply(state::actions::SetLunarMonth{month});
					}
					float phase = input.dayNight.moonPhaseDays;
					if (ImGui::SliderFloat("Moon Phase (days)", &phase, 0.0f, month, "%.1f")) {
						fb->Apply(state::actions::SetMoonPhaseDays{phase});
					}
					ImGui::Text(
						"Phase: %.0f%%",
						(std::fmod(phase, month) / month) * 100.0f
					);
				}

				// 2. Atmosphere
				if (ImGui::CollapsingHeader("Atmosphere", ImGuiTreeNodeFlags_DefaultOpen)) {
					bool at_enabled = input.atmosphere.enabled;
					if (ImGui::Checkbox("Enable Atmosphere", &at_enabled)) {
						fb->Apply(state::actions::SetAtmosphereEnabled{at_enabled});
					}

					if (at_enabled) {
						float haze_density = input.atmosphere.hazeDensity;
						if (SliderFloatWithActual("Haze Density", &haze_density, eff.atmosphere.hazeDensity, 0.0f, 5.0f)) {
							fb->Apply(state::actions::SetHazeDensity{haze_density});
						}
						if (weather) {
							ImGui::SameLine();
							if (ImGui::Button("Unlock##HazeDensity")) weather->ClearTarget(WeatherAttribute::HazeDensity);
						}

						float haze_height = input.atmosphere.hazeHeight;
						if (SliderFloatWithActual("Haze Height", &haze_height, eff.atmosphere.hazeHeight, 0.0f, 50.0f)) {
							fb->Apply(state::actions::SetHazeHeight{haze_height});
						}
						if (weather) {
							ImGui::SameLine();
							if (ImGui::Button("Unlock##HazeHeight")) weather->ClearTarget(WeatherAttribute::HazeHeight);
						}

						glm::vec3 haze_color = input.atmosphere.hazeColor;
						if (ImGui::ColorEdit3("Haze Color", &haze_color[0])) {
							fb->Apply(state::actions::SetHazeColor{haze_color});
						}
						if (weather) {
							ImGui::SameLine();
							if (ImGui::Button("Unlock##HazeColor")) {
								weather->ClearTarget(WeatherAttribute::HazeColorR);
								weather->ClearTarget(WeatherAttribute::HazeColorG);
								weather->ClearTarget(WeatherAttribute::HazeColorB);
							}
						}

						float cloud_density = input.atmosphere.cloudDensity;
						if (SliderFloatWithActual("Cloud Density", &cloud_density, eff.atmosphere.cloudDensity, 0.0f, 1.0f)) {
							fb->Apply(state::actions::SetCloudDensity{cloud_density});
						}
						if (weather) {
							ImGui::SameLine();
							if (ImGui::Button("Unlock##CloudDensity")) weather->ClearTarget(WeatherAttribute::CloudDensity);
						}

						float cloud_altitude = input.atmosphere.cloudAltitude;
						if (SliderFloatWithActual("Cloud Altitude", &cloud_altitude, eff.atmosphere.cloudAltitude, 0.0f, 1000.0f)) {
							fb->Apply(state::actions::SetCloudAltitude{cloud_altitude});
						}
						if (weather) {
							ImGui::SameLine();
							if (ImGui::Button("Unlock##CloudAltitude")) weather->ClearTarget(WeatherAttribute::CloudAltitude);
						}

						float cloud_thickness = input.atmosphere.cloudThickness;
						if (SliderFloatWithActual("Cloud Thickness", &cloud_thickness, eff.atmosphere.cloudThickness, 0.0f, 500.0f)) {
							fb->Apply(state::actions::SetCloudThickness{cloud_thickness});
						}
						if (weather) {
							ImGui::SameLine();
							if (ImGui::Button("Unlock##CloudThickness")) weather->ClearTarget(WeatherAttribute::CloudThickness);
						}

						float cloud_coverage = input.atmosphere.cloudCoverage;
						if (SliderFloatWithActual("Cloud Coverage", &cloud_coverage, eff.atmosphere.cloudCoverage, 0.0f, 2.0f)) {
							fb->Apply(state::actions::SetCloudCoverage{cloud_coverage});
						}
						if (weather) {
							ImGui::SameLine();
							if (ImGui::Button("Unlock##CloudCoverage")) weather->ClearTarget(WeatherAttribute::CloudCoverage);
						}

						float cloud_warp = input.atmosphere.cloudWarp;
						if (ImGui::SliderFloat("Camera Cloud Buffer", &cloud_warp, 0.0f, 1000.0f)) {
							fb->Apply(state::actions::SetCloudWarp{cloud_warp});
						}

						glm::vec3 cloud_color = input.atmosphere.cloudColor;
						if (ImGui::ColorEdit3("Cloud Color", &cloud_color[0])) {
							fb->Apply(state::actions::SetCloudColor{cloud_color});
						}
						if (weather) {
							ImGui::SameLine();
							if (ImGui::Button("Unlock##CloudColor")) {
								weather->ClearTarget(WeatherAttribute::CloudColorR);
								weather->ClearTarget(WeatherAttribute::CloudColorG);
								weather->ClearTarget(WeatherAttribute::CloudColorB);
							}
						}

						float cloud_shadow = input.atmosphere.cloudShadowIntensity;
						if (ImGui::SliderFloat("Cloud Shadow Intensity", &cloud_shadow, 0.0f, 1.0f)) {
							fb->Apply(state::actions::SetCloudShadowIntensity{cloud_shadow});
						}

						ImGui::Separator();
						ImGui::Text("Advanced Cloud Parameters");

						auto& cfg = ConfigManager::GetInstance();

						float g1 = cfg.GetAppSettingFloat("cloud_phase_g1", 0.8f);
						if (ImGui::SliderFloat("Phase G1 (Forward)", &g1, 0.0f, 0.99f)) {
							cfg.SetFloat("cloud_phase_g1", g1);
						}
						float g2 = cfg.GetAppSettingFloat("cloud_phase_g2", -0.3f);
						if (ImGui::SliderFloat("Phase G2 (Backward)", &g2, -0.99f, 0.0f)) {
							cfg.SetFloat("cloud_phase_g2", g2);
						}
						float alpha = cfg.GetAppSettingFloat("cloud_phase_alpha", 0.7f);
						if (ImGui::SliderFloat("Phase Mix Alpha", &alpha, 0.0f, 1.0f)) {
							cfg.SetFloat("cloud_phase_alpha", alpha);
						}
						float isotropic = cfg.GetAppSettingFloat("cloud_phase_isotropic", 0.15f);
						if (ImGui::SliderFloat("Phase Isotropic Mix", &isotropic, 0.0f, 1.0f)) {
							cfg.SetFloat("cloud_phase_isotropic", isotropic);
						}

						float p_scale = input.atmosphere.cloudPowderScale;
						if (ImGui::SliderFloat("Powder Scale", &p_scale, 0.0f, 2.0f)) {
							fb->Apply(state::actions::SetCloudPowderScale{p_scale});
						}
						float p_mult = input.atmosphere.cloudPowderMultiplier;
						if (ImGui::SliderFloat("Powder Multiplier", &p_mult, 0.0f, 2.0f)) {
							fb->Apply(state::actions::SetCloudPowderMultiplier{p_mult});
						}
						float p_local = input.atmosphere.cloudPowderLocalScale;
						if (ImGui::SliderFloat("Powder Local Scale", &p_local, 0.0f, 10.0f)) {
							fb->Apply(state::actions::SetCloudPowderLocalScale{p_local});
						}

						float s_opt = input.atmosphere.cloudShadowOpticalDepthMultiplier;
						if (ImGui::SliderFloat("Shadow Optical Depth Mult", &s_opt, 0.0f, 1.0f)) {
							fb->Apply(state::actions::SetCloudShadowOpticalDepthMultiplier{s_opt});
						}
						float s_step = input.atmosphere.cloudShadowStepMultiplier;
						if (ImGui::SliderFloat("Shadow Step Mult", &s_step, 0.0f, 1.0f)) {
							fb->Apply(state::actions::SetCloudShadowStepMultiplier{s_step});
						}

						float sun_scale = input.atmosphere.cloudSunLightScale;
						if (ImGui::SliderFloat("Sun Light Scale", &sun_scale, 0.0f, 50.0f)) {
							fb->Apply(state::actions::SetCloudSunLightScale{sun_scale});
						}
						float moon_scale = input.atmosphere.cloudMoonLightScale;
						if (ImGui::SliderFloat("Moon Light Scale", &moon_scale, 0.0f, 20.0f)) {
							fb->Apply(state::actions::SetCloudMoonLightScale{moon_scale});
						}

						float bp_mix = input.atmosphere.cloudBeerPowderMix;
						if (ImGui::SliderFloat("Beer-Powder Mix", &bp_mix, 0.0f, 1.0f)) {
							fb->Apply(state::actions::SetCloudBeerPowderMix{bp_mix});
						}

						float flow_speed = input.atmosphere.cloudFlowSpeed;
						if (ImGui::SliderFloat("Flow Speed", &flow_speed, 0.0f, 0.5f)) {
							fb->Apply(state::actions::SetCloudFlowSpeed{flow_speed});
						}

						float flow_dir = input.atmosphere.cloudFlowDirection;
						if (ImGui::SliderAngle("Flow Direction", &flow_dir)) {
							fb->Apply(state::actions::SetCloudFlowDirection{flow_dir});
						}

						float flow_height = input.atmosphere.cloudFlowHeightScale;
						if (ImGui::SliderFloat("Flow Height Scale", &flow_height, 0.0f, 5.0f)) {
							fb->Apply(state::actions::SetCloudFlowHeightScale{flow_height});
						}

						float curl_strength = input.atmosphere.cloudCurlStrength;
						if (ImGui::SliderFloat("Curl Strength", &curl_strength, 0.0f, 20.0f)) {
							fb->Apply(state::actions::SetCloudCurlStrength{curl_strength});
						}

						float curl_freq = input.atmosphere.cloudCurlFrequency;
						float curl_freq_scaled = curl_freq * 900.0f; // Show as relative to default
						if (ImGui::SliderFloat("Curl Frequency Scale", &curl_freq_scaled, 0.1f, 10.0f)) {
							fb->Apply(state::actions::SetCloudCurlFrequency{curl_freq_scaled / 900.0f});
						}

						ImGui::Separator();
						ImGui::Text("Scattering");
						float rayleigh = input.atmosphere.rayleighScale;
						if (SliderFloatWithActual("Rayleigh Scale", &rayleigh, eff.atmosphere.rayleighScale, 0.0f, 3.0f)) {
							fb->Apply(state::actions::SetRayleighScale{rayleigh});
						}
						if (weather) {
							ImGui::SameLine();
							if (ImGui::Button("Unlock##RayleighScale")) weather->ClearTarget(WeatherAttribute::RayleighScale);
						}
						float mie = input.atmosphere.mieScale;
						if (SliderFloatWithActual("Mie Scale", &mie, eff.atmosphere.mieScale, 0.0f, 0.25f)) {
							fb->Apply(state::actions::SetMieScale{mie});
						}
						if (weather) {
							ImGui::SameLine();
							if (ImGui::Button("Unlock##MieScale")) weather->ClearTarget(WeatherAttribute::MieScale);
						}
						float mie_g = input.atmosphere.mieAnisotropy;
						if (ImGui::SliderFloat("Mie Anisotropy", &mie_g, 0.0f, 0.99f)) {
							fb->Apply(state::actions::SetMieAnisotropy{mie_g});
						}
						float multi_scat = input.atmosphere.multiScatScale;
						if (ImGui::SliderFloat("MultiScat Scale", &multi_scat, 0.0f, 0.25f)) {
							fb->Apply(state::actions::SetMultiScatScale{multi_scat});
						}
						float ambient_scat = input.atmosphere.ambientScatScale;
						if (ImGui::SliderFloat("Ambient Scat Scale", &ambient_scat, 0.0f, 2.0f)) {
							fb->Apply(state::actions::SetAmbientScatScale{ambient_scat});
						}

						ImGui::Separator();
						ImGui::Text("Physical Parameters");

						float atmos_height = input.atmosphere.atmosphereHeight;
						if (ImGui::SliderFloat("Atmosphere Height (km)", &atmos_height, 0.0f, 300.0f)) {
							fb->Apply(state::actions::SetAtmosphereHeight{atmos_height});
						}
						if (weather) {
							ImGui::SameLine();
							if (ImGui::Button("Unlock##AtmosphereHeight")) weather->ClearTarget(WeatherAttribute::AtmosphereHeight);
						}

						glm::vec3 rayleigh_scattering = input.atmosphere.rayleighScattering * 1000.0f;
						if (ImGui::ColorEdit3("Rayleigh Scattering", &rayleigh_scattering[0])) {
							fb->Apply(state::actions::SetRayleighScattering{rayleigh_scattering * 0.001f});
						}
						if (weather) {
							ImGui::SameLine();
							if (ImGui::Button("Unlock##RayleighScattering")) {
								weather->ClearTarget(WeatherAttribute::RayleighScatteringR);
								weather->ClearTarget(WeatherAttribute::RayleighScatteringG);
								weather->ClearTarget(WeatherAttribute::RayleighScatteringB);
							}
						}

						float mie_scat = input.atmosphere.mieScattering * 1000.0f;
						if (ImGui::SliderFloat("Mie Scattering coeff", &mie_scat, 0.0f, 10.0f)) {
							fb->Apply(state::actions::SetMieScattering{mie_scat * 0.001f});
						}
						if (weather) {
							ImGui::SameLine();
							if (ImGui::Button("Unlock##MieScattering")) weather->ClearTarget(WeatherAttribute::MieScattering);
						}

						float mie_ext = input.atmosphere.mieExtinction * 1000.0f;
						if (ImGui::SliderFloat("Mie Extinction coeff", &mie_ext, 0.0f, 10.0f)) {
							fb->Apply(state::actions::SetMieExtinction{mie_ext * 0.001f});
						}
						if (weather) {
							ImGui::SameLine();
							if (ImGui::Button("Unlock##MieExtinction")) weather->ClearTarget(WeatherAttribute::MieExtinction);
						}

						glm::vec3 ozone_absorption = input.atmosphere.ozoneAbsorption * 1000.0f;
						if (ImGui::ColorEdit3("Ozone Absorption", &ozone_absorption[0])) {
							fb->Apply(state::actions::SetOzoneAbsorption{ozone_absorption * 0.001f});
						}

						float rayleigh_h = input.atmosphere.rayleighScaleHeight;
						if (ImGui::SliderFloat("Rayleigh Scale Height (km)", &rayleigh_h, 0.0f, 20.0f)) {
							fb->Apply(state::actions::SetRayleighScaleHeight{rayleigh_h});
						}
						if (weather) {
							ImGui::SameLine();
							if (ImGui::Button("Unlock##RayleighScaleHeight")) weather->ClearTarget(WeatherAttribute::RayleighScaleHeight);
						}

						float mie_h = input.atmosphere.mieScaleHeight;
						if (ImGui::SliderFloat("Mie Scale Height (km)", &mie_h, 0.0f, 3.0f)) {
							fb->Apply(state::actions::SetMieScaleHeight{mie_h});
						}
						if (weather) {
							ImGui::SameLine();
							if (ImGui::Button("Unlock##MieScaleHeight")) weather->ClearTarget(WeatherAttribute::MieScaleHeight);
						}

						ImGui::Separator();
						ImGui::Text("Atmosphere Variance");

						float var_scale = input.atmosphere.colorVarianceScale;
						if (ImGui::SliderFloat("Variance Scale", &var_scale, 0.0f, 2.5f)) {
							fb->Apply(state::actions::SetAtmosphereColorVarianceScale{var_scale});
						}

						float var_strength = input.atmosphere.colorVarianceStrength;
						if (ImGui::SliderFloat("Variance Strength", &var_strength, 0.0f, 0.5f)) {
							fb->Apply(state::actions::SetAtmosphereColorVarianceStrength{var_strength});
						}
					}
				}

				// 2.5 Volumetric Lighting
				if (ImGui::CollapsingHeader("Volumetric Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
					bool vol_enabled = input.volumetric.enabled;
					if (ImGui::Checkbox("Enable Volumetric Lighting", &vol_enabled)) {
						fb->Apply(state::actions::SetVolumetricEnabled{vol_enabled});
					}

					if (vol_enabled) {
						float intensity = input.volumetric.intensity;
						if (ImGui::SliderFloat("Intensity##Vol", &intensity, 0.0f, 5.0f)) {
							fb->Apply(state::actions::SetVolumetricIntensity{intensity});
						}
						float anisotropy = input.volumetric.anisotropy;
						if (ImGui::SliderFloat("Anisotropy##Vol", &anisotropy, 0.0f, 0.99f)) {
							fb->Apply(state::actions::SetVolumetricAnisotropy{anisotropy});
						}
						float vol_alpha = input.volumetric.temporalAlpha;
						if (ImGui::SliderFloat("Temporal Alpha##Vol", &vol_alpha, 0.0f, 0.99f)) {
							fb->Apply(state::actions::SetVolumetricTemporalAlpha{vol_alpha});
						}
					}
				}

				// 3. Terrain & Foliage
				if (ImGui::CollapsingHeader("Terrain", ImGuiTreeNodeFlags_DefaultOpen)) {
					bool render_terrain = input.terrain.renderTerrain;
					if (ImGui::Checkbox("Render Terrain", &render_terrain)) {
						fb->Apply(state::actions::SetRenderTerrain{render_terrain});
					}
					bool render_floor = input.terrain.renderFloor;
					if (ImGui::Checkbox("Render Floor", &render_floor)) {
						fb->Apply(state::actions::SetRenderFloor{render_floor});
					}
					bool force_both = input.terrain.forceBoth;
					if (ImGui::Checkbox("Force Both Floor and Terrain", &force_both)) {
						fb->Apply(state::actions::SetForceBoth{force_both});
					}

					ImGui::Separator();

					float world_scale = input.terrain.worldScale;
					if (ImGui::SliderFloat("World Scale", &world_scale, 0.1f, 5.0f)) {
						fb->Apply(state::actions::SetWorldScale{world_scale});
					}
					ImGui::Text("Higher = larger world, Lower = smaller world");

					bool fol_enabled = input.terrain.foliageEnabled;
					if (ImGui::Checkbox("Enable Foliage", &fol_enabled)) {
						fb->Apply(state::actions::SetFoliageEnabled{fol_enabled});
					}

					if (fol_enabled) {
						float threshold = input.terrain.foliagePixelThreshold;
						if (ImGui::SliderFloat("Foliage Pixel Threshold", &threshold, 0.0f, 50.0f)) {
							fb->Apply(state::actions::SetFoliagePixelThreshold{threshold});
						}
						ImGui::Text("Higher = cull larger objects, 0 = disable size culling");
					}
				}

				// 4. Particles
				if (ImGui::CollapsingHeader("Particles", ImGuiTreeNodeFlags_DefaultOpen)) {
					bool part_enabled = input.particles.enabled;
					if (ImGui::Checkbox("Enable Particle System", &part_enabled)) {
						fb->Apply(state::actions::SetParticlesEnabled{part_enabled});
					}

					if (part_enabled) {
						float density = input.particles.ambientDensity;
						if (ImGui::SliderFloat("Ambient Density", &density, 0.0f, 2.0f, "%.2f")) {
							fb->Apply(state::actions::SetAmbientParticleDensity{density});
						}
						ImGui::Text(
							"Approx. %d ambient particles",
							(int)(density * Constants::Class::Particles::AmbientParticleScale())
						);

						ImGui::Separator();
						ImGui::Text("Population Limits & Live Counts");

						auto drawRatioLimit = [&](const char* label, const char* type, float target_ratio, uint32_t current, uint32_t limit) {
							float ratio = target_ratio;
							if (ImGui::SliderFloat(label, &ratio, 0.0f, 1.0f, "%.2f")) {
								fb->Apply(state::actions::SetParticleRatio{type, ratio});
							}
							ImGui::SameLine();
							ImGui::Text("(%u/%u live)", current, limit);
						};

						drawRatioLimit("Birds", "birds", input.particles.ratioBirds, sim.particles.countBirds, sim.particles.limitBirds);
						drawRatioLimit("Leaves", "leaves", input.particles.ratioLeaves, sim.particles.countLeaves, sim.particles.limitLeaves);
						drawRatioLimit("Petals", "petals", input.particles.ratioPetals, sim.particles.countPetals, sim.particles.limitPetals);
						drawRatioLimit("Bubbles", "bubbles", input.particles.ratioBubbles, sim.particles.countBubbles, sim.particles.limitBubbles);
						drawRatioLimit("Fireflies", "fireflies", input.particles.ratioFireflies, sim.particles.countFireflies, sim.particles.limitFireflies);
						drawRatioLimit("Fairies", "fairies", input.particles.ratioFairies, sim.particles.countFairies, sim.particles.limitFairies);
						drawRatioLimit("Snow", "snow", input.particles.ratioSnow, sim.particles.countSnow, sim.particles.limitSnow);
						drawRatioLimit("Rain", "rain", input.particles.ratioRain, sim.particles.countRain, sim.particles.limitRain);
						drawRatioLimit("Dust", "dust", input.particles.ratioDust, sim.particles.countDust, sim.particles.limitDust);
					}
				}

				// 5. Wind
				if (ImGui::CollapsingHeader("Wind", ImGuiTreeNodeFlags_DefaultOpen)) {
					if (weather) {
						drawAttrControl("Wind Strength", "windStrength", input.weather.windStrength, sim.weather.windStrength, 0.0f, 5.0f, "%.2f", [](float v){ return state::actions::SetWeatherWindStrength{v}; });
						drawAttrControl("Wind Speed", "windSpeed", input.weather.windSpeed, sim.weather.windSpeed, 0.0f, 10.0f, "%.2f", [](float v){ return state::actions::SetWeatherWindSpeed{v}; });
						drawAttrControl("Wind Frequency", "windFrequency", input.weather.windFrequency, sim.weather.windFrequency, 0.01f, 1.0f, "%.2f", [](float v){ return state::actions::SetWeatherWindFrequency{v}; });
					} else {
						float wind_strength = input.weather.windStrength;
						if (ImGui::SliderFloat("Wind Strength", &wind_strength, 0.0f, 5.0f)) {
							fb->Apply(state::actions::SetWeatherWindStrength{wind_strength});
						}

						float wind_speed = input.weather.windSpeed;
						if (ImGui::SliderFloat("Wind Speed", &wind_speed, 0.0f, 10.0f)) {
							fb->Apply(state::actions::SetWeatherWindSpeed{wind_speed});
						}

						float wind_frequency = input.weather.windFrequency;
						if (ImGui::SliderFloat("Wind Frequency", &wind_frequency, 0.01f, 1.0f)) {
							fb->Apply(state::actions::SetWeatherWindFrequency{wind_frequency});
						}
					}
				}

				// 6. Terrain Erosion
				if (ImGui::CollapsingHeader("Terrain Erosion", ImGuiTreeNodeFlags_DefaultOpen)) {
					bool erosion_enabled = input.erosion.enabled;
					if (ImGui::Checkbox("Enable Erosion Filter", &erosion_enabled)) {
						fb->Apply(state::actions::SetErosionEnabled{erosion_enabled});
					}

					if (erosion_enabled) {
						float erosion_strength = input.erosion.strength;
						if (ImGui::SliderFloat("Erosion Strength", &erosion_strength, 0.0f, 0.5f, "%.3f")) {
							fb->Apply(state::actions::SetErosionStrength{erosion_strength});
						}

						float erosion_scale = input.erosion.scale;
						if (ImGui::SliderFloat("Erosion Scale", &erosion_scale, 0.01f, 1.0f, "%.2f")) {
							fb->Apply(state::actions::SetErosionScale{erosion_scale});
						}

						float erosion_detail = input.erosion.detail;
						if (ImGui::SliderFloat("Erosion Detail", &erosion_detail, 0.1f, 5.0f, "%.1f")) {
							fb->Apply(state::actions::SetErosionDetail{erosion_detail});
						}

						float erosion_gully_weight = input.erosion.gullyWeight;
						if (ImGui::SliderFloat("Gully Weight", &erosion_gully_weight, 0.0f, 1.0f, "%.2f")) {
							fb->Apply(state::actions::SetErosionGullyWeight{erosion_gully_weight});
						}

						float erosion_max_dist = input.erosion.maxDist;
						if (ImGui::SliderFloat("Erosion Max Dist", &erosion_max_dist, 50.0f, 1000.0f, "%.0f")) {
							fb->Apply(state::actions::SetErosionMaxDist{erosion_max_dist});
						}
					}
				}
			}
			ImGui::End();
		}
	} // namespace UI
} // namespace Boidsish
