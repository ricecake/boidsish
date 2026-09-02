#include "ui/EnvironmentWidget.h"

#include <cmath>

#include "ConfigManager.h"
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
#include "service_locator.h"
#include "atmosphere_manager.h"

namespace Boidsish {
	namespace UI {

		EnvironmentWidget::EnvironmentWidget(Visualizer& visualizer): m_visualizer(visualizer) {}

		void EnvironmentWidget::Draw() {
			if (!m_show)
				return;

			ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("Environment", &m_show)) {
				auto weather = m_visualizer.GetWeatherManager();
				auto drawAttrControl = [&](const char* label, WeatherAttribute attr, float min_v, float max_v, const char* fmt) {
					if (!weather) return;
					float val = 0.0f;
					// Use current value for the slider if no target is set
					const auto& weather_cur = weather->GetCurrentWeather();
					switch(attr) {
						case WeatherAttribute::Temperature: val = weather_cur.temperature; break;
						case WeatherAttribute::Precipitation: val = weather_cur.precipitation; break;
						case WeatherAttribute::Humidity: val = weather_cur.humidity; break;
						case WeatherAttribute::WindStrength: val = weather_cur.wind_strength; break;
						case WeatherAttribute::WindSpeed: val = weather_cur.wind_speed; break;
						case WeatherAttribute::WindFrequency: val = weather_cur.wind_frequency; break;
						case WeatherAttribute::CloudCoverage: val = weather_cur.cloud_coverage; break;
						case WeatherAttribute::SunAureoleStrength: val = weather_cur.sun_aureole_strength; break;
						case WeatherAttribute::CirrusOpacity: val = weather_cur.cirrus_opacity; break;
						default: break;
					}

					ImGui::Text("%s:", label);
					if (ImGui::SliderFloat((std::string("##slide") + label).c_str(), &val, min_v, max_v, fmt)) {
						weather->SetTarget(attr, val);
					}
					ImGui::SameLine();
					if (ImGui::Button((std::string("T##") + label).c_str())) weather->SetTarget(attr, val);
					ImGui::SetItemTooltip("Set Target");
					ImGui::SameLine();
					if (ImGui::Button((std::string("min##") + label).c_str())) weather->SetMin(attr, val);
					ImGui::SetItemTooltip("Set Minimum");
					ImGui::SameLine();
					if (ImGui::Button((std::string("max##") + label).c_str())) weather->SetMax(attr, val);
					ImGui::SetItemTooltip("Set Maximum");
					ImGui::SameLine();
					if (ImGui::Button((std::string("U##") + label).c_str())) {
						weather->ClearTarget(attr);
						weather->ClearMin(attr);
						weather->ClearMax(attr);
					}
					ImGui::SetItemTooltip("Unlock / Clear Constraints");
				};

				// 0. Weather
				if (ImGui::CollapsingHeader("Ambient Weather", ImGuiTreeNodeFlags_DefaultOpen)) {
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

							if (ImGui::Button("Clear All Weather Constraints")) {
								weather->ClearAllConstraints();
							}

							ImGui::Separator();
							const auto& cur = weather->GetCurrentWeather();

							drawAttrControl("Temperature (K)", WeatherAttribute::Temperature, 200.0f, 350.0f, "%.1f K");
							float temp = cur.temperature;
							ImGui::Text("  (%.1f C / %.1f F)", temp - 273.15f, (temp - 273.15f) * 1.8f + 32.0f);

							drawAttrControl("Precipitation", WeatherAttribute::Precipitation, 0.0f, 1.0f, "%.2f");
							drawAttrControl("Humidity", WeatherAttribute::Humidity, 0.0f, 1.0f, "%.2f");
							drawAttrControl("Cloud Coverage", WeatherAttribute::CloudCoverage, 0.0f, 1.0f, "%.2f");
							drawAttrControl("Sun Aureole", WeatherAttribute::SunAureoleStrength, 0.0f, 2.0f, "%.2f");
							drawAttrControl("Cirrus Opacity", WeatherAttribute::CirrusOpacity, 0.0f, 1.0f, "%.2f");
							drawAttrControl("Wind Strength", WeatherAttribute::WindStrength, 0.0f, 5.0f, "%.2f");
							drawAttrControl("Wind Speed", WeatherAttribute::WindSpeed, 0.0f, 10.0f, "%.2f");
							drawAttrControl("Wind Frequency", WeatherAttribute::WindFrequency, 0.01f, 1.0f, "%.2f");


							auto active_constraints = weather->GetActiveConstraints();
							if (!active_constraints.empty()) {
								if (ImGui::CollapsingHeader("Active Weather Constraints", ImGuiTreeNodeFlags_None)) {
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
							}

							// Display derived intensities and wetness
							float rain = (cur.temperature > 273.15f) ? cur.precipitation : 0.0f;
							float snow = (cur.temperature <= 273.15f) ? cur.precipitation : 0.0f;
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
								bool macro_enabled = weather->IsMacroSimEnabled();
								if (ImGui::Checkbox("Enable Macro Simulation", &macro_enabled)) {
									weather->SetMacroSimEnabled(macro_enabled);
								}

								if (macro_enabled) {
									float tau = weather->GetSimTau();
									if (ImGui::SliderFloat("Simulation Relaxation (Tau)", &tau, 0.51f, 2.0f, "%.2f")) {
										weather->SetSimTau(tau);
									}

									bool strict = weather->IsStrictEnforcementEnabled();
									if (ImGui::Checkbox("Strict Enforcement (Instant Snap)", &strict)) {
										weather->SetStrictEnforcement(strict);
									}

									float stiffness = weather->GetNudgeStiffness();
									if (ImGui::SliderFloat("Nudge Stiffness", &stiffness, 0.01f, 10.0f, "%.2f")) {
										weather->SetNudgeStiffness(stiffness);
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

								if (ImGui::TreeNode("Climate & Edge Boundary Controls")) {
									auto climate = weather->GetClimateSettings();
									bool changed = false;

									ImGui::Text("Macro Wind & Edge Boundaries:");
									if (ImGui::SliderFloat("Macro Wind Speed (m/s)", &climate.macroWindSpeed, 0.0f, 50.0f, "%.1f m/s")) changed = true;
									if (ImGui::SliderFloat("Macro Wind Direction", &climate.macroWindDirection, 0.0f, 360.0f, "%.1f deg")) changed = true;
									if (ImGui::SliderFloat("Incoming Air Pressure (hPa)", &climate.boundaryPressure, 800.0f, 1200.0f, "%.1f hPa")) changed = true;
									if (ImGui::SliderFloat("Incoming Air Temp (K)", &climate.boundaryTemperature, 200.0f, 350.0f, "%.1f K")) changed = true;
									ImGui::Text("  (%.1f C / %.1f F)", climate.boundaryTemperature - 273.15f, (climate.boundaryTemperature - 273.15f) * 1.8f + 32.0f);

									ImGui::Separator();
									ImGui::Text("Thermodynamics & Sensible Heat:");
									if (ImGui::SliderFloat("Sensible Heat Multiplier", &climate.sensibleHeatMultiplier, 0.0f, 5.0f, "%.2f")) changed = true;
									if (ImGui::SliderFloat("Solar Heating Scale", &climate.solarHeatingScale, 0.0f, 5.0f, "%.2f")) changed = true;
									if (ImGui::SliderFloat("Evaporation Scale", &climate.evaporationScale, 0.0f, 5.0f, "%.2f")) changed = true;

									ImGui::Separator();
									ImGui::Text("Baseline Atmosphere & Aerosols:");
									if (ImGui::SliderFloat("Baseline Humidity", &climate.baselineHumidity, 0.0f, 1.0f, "%.2f")) changed = true;
									if (ImGui::SliderFloat("Aerosol Emission Scale", &climate.aerosolEmissionScale, 0.0f, 5.0f, "%.2f")) changed = true;
									if (ImGui::SliderFloat("Baseline Aerosol Level", &climate.baselineAerosolLevel, 0.0f, 0.5f, "%.3f")) changed = true;

									if (ImGui::Button("Reset Climate Defaults")) {
										climate = ClimateSettings{};
										changed = true;
									}

									if (changed) {
										weather->SetClimateSettings(climate);
									}

									ImGui::TreePop();
								}

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
											weather->InjectPressure(get_target_pos(), m_targetPressure, m_burstStrength);
										}
										ImGui::SameLine();
										if (ImGui::Button("Burst (1100 hPa)")) {
											weather->InjectPressure(get_target_pos(), 1100.0f, 0.1f);
										}
										ImGui::SameLine();
										if (ImGui::Button("Vacuum (900 hPa)")) {
											weather->InjectPressure(get_target_pos(), 900.0f, 0.0f);
										}

										if (ImGui::Button("Inject Temperature")) {
											weather->InjectTemperature(get_target_pos(), m_targetTemperature);
										}
										ImGui::SameLine();
										if (ImGui::Button("Heat (350 K)")) {
											weather->InjectTemperature(get_target_pos(), 350.0f);
										}
										ImGui::SameLine();
										if (ImGui::Button("Cold (250 K)")) {
											weather->InjectTemperature(get_target_pos(), 250.0f);
										}

										if (ImGui::Button("Inject Aerosol")) {
											weather->InjectAerosol(get_target_pos(), m_targetAerosol);
										}
										ImGui::SameLine();
										if (ImGui::Button("Dusty (0.5)")) {
											weather->InjectAerosol(get_target_pos(), 0.5f);
										}
										ImGui::SameLine();
										if (ImGui::Button("Clean (0.0)")) {
											weather->InjectAerosol(get_target_pos(), 0.0f);
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
				if (ImGui::CollapsingHeader("Foliage & Grass", ImGuiTreeNodeFlags_DefaultOpen)) {
					auto grass_manager = m_visualizer.GetGrassManager();
					if (grass_manager) {
						auto& cfg = ConfigManager::GetInstance();
						bool  enabled = grass_manager->IsEnabled();
						if (ImGui::Checkbox("Enable Foliage & Grass", &enabled)) {
							grass_manager->SetEnabled(enabled);
							cfg.SetBool("grass_enabled", enabled);
						}

						if (enabled) {
							GlobalGrassProperties props = grass_manager->GetGlobalProperties();
							bool                  changed = false;

							if (ImGui::SliderFloat("Length Multiplier", &props.lengthMultiplier, 0.1f, 5.0f)) {
								changed = true;
								cfg.SetFloat("grass_length_multiplier", props.lengthMultiplier);
							}
							if (ImGui::SliderFloat("Width Multiplier", &props.widthMultiplier, 0.1f, 5.0f)) {
								changed = true;
								cfg.SetFloat("grass_width_multiplier", props.widthMultiplier);
							}
							if (ImGui::SliderFloat("Density Multiplier", &props.densityMultiplier, 0.0f, 2.0f)) {
								changed = true;
								cfg.SetFloat("grass_density_multiplier", props.densityMultiplier);
							}
							if (ImGui::SliderFloat("Rigidity Multiplier", &props.rigidityMultiplier, 0.0f, 2.0f)) {
								changed = true;
								cfg.SetFloat("grass_rigidity_multiplier", props.rigidityMultiplier);
							}
							if (ImGui::SliderFloat("Wind Multiplier", &props.windMultiplier, 0.0f, 5.0f)) {
								changed = true;
								cfg.SetFloat("grass_wind_multiplier", props.windMultiplier);
							}

							ImGui::Separator();
							ImGui::Text("Procedural Scaling");
							if (ImGui::SliderFloat("LOD Scale Factor", &props.lodScaleFactor, 1.1f, 4.0f)) {
								changed = true;
								cfg.SetFloat("grass_lod_scale_factor", props.lodScaleFactor);
							}
							if (ImGui::SliderFloat("LOD Base Range", &props.lodBaseRange, 5.0f, 100.0f)) {
								changed = true;
								cfg.SetFloat("grass_lod_base_range", props.lodBaseRange);
							}
							if (ImGui::SliderFloat("Base Grid Scale", &props.baseScale, 0.05f, 2.0f)) {
								changed = true;
								cfg.SetFloat("grass_base_scale", props.baseScale);
							}

							if (changed) {
								grass_manager->SetGlobalProperties(props);
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

					ImGui::Separator();
					ImGui::Text("Moon");
					ImGui::SliderFloat("Lunar Albedo", &cycle.lunar_albedo, 0.0f, 0.5f, "%.3f");
					ImGui::ColorEdit3("Moon Tint", &cycle.moon_tint[0], ImGuiColorEditFlags_HDR|ImGuiColorEditFlags_Float);
					ImGui::SliderFloat("Lunar Month (days)", &cycle.lunar_month, 0.1f, 30.0f, "%.2f");
					ImGui::SliderFloat("Moon Phase (days)", &cycle.moon_phase_days, 0.0f, cycle.lunar_month, "%.1f");
					ImGui::Text(
						"Phase: %.0f%%",
						(std::fmod(cycle.moon_phase_days, cycle.lunar_month) / cycle.lunar_month) * 100.0f
					);
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
									auto weather = m_visualizer.GetWeatherManager();
									bool changed_atm = false;

									float haze_density = atmosphere_effect->GetHazeDensity();
									if (ImGui::SliderFloat("Haze Density", &haze_density, 0.0f, 5.0f, "%.2f")) {
										if (weather) weather->SetTarget(WeatherAttribute::HazeDensity, haze_density);
										else atmosphere_effect->SetHazeDensity(haze_density);
										changed_atm = true;
									}
									if (weather) {
										ImGui::SameLine();
										if (ImGui::Button("Unlock##HazeDensity")) weather->ClearTarget(WeatherAttribute::HazeDensity);
									}
									float haze_height = atmosphere_effect->GetHazeHeight();
									if (ImGui::SliderFloat("Haze Height", &haze_height, 0.0f, 50.0f)) {
										if (weather) weather->SetTarget(WeatherAttribute::HazeHeight, haze_height);
										else atmosphere_effect->SetHazeHeight(haze_height);
										changed_atm = true;
									}
									if (weather) {
										ImGui::SameLine();
										if (ImGui::Button("Unlock##HazeHeight")) weather->ClearTarget(WeatherAttribute::HazeHeight);
									}
									glm::vec3 haze_color = atmosphere_effect->GetHazeColor();
									if (ImGui::ColorEdit3("Haze Color", &haze_color[0], ImGuiColorEditFlags_HDR|ImGuiColorEditFlags_Float)) {
										if (weather) {
											weather->SetTarget(WeatherAttribute::HazeColorR, haze_color.r);
											weather->SetTarget(WeatherAttribute::HazeColorG, haze_color.g);
											weather->SetTarget(WeatherAttribute::HazeColorB, haze_color.b);
										} else atmosphere_effect->SetHazeColor(haze_color);
										changed_atm = true;
									}
									if (weather) {
										ImGui::SameLine();
										if (ImGui::Button("Unlock##HazeColor")) {
											weather->ClearTarget(WeatherAttribute::HazeColorR);
											weather->ClearTarget(WeatherAttribute::HazeColorG);
											weather->ClearTarget(WeatherAttribute::HazeColorB);
										}
									}
									float cloud_density = atmosphere_effect->GetCloudDensity();
									if (ImGui::SliderFloat("Cloud Density", &cloud_density, 0.0f, 1.0f)) {
										if (weather) weather->SetTarget(WeatherAttribute::CloudDensity, cloud_density);
										else atmosphere_effect->SetCloudDensity(cloud_density);
										changed_atm = true;
									}
									if (weather) {
										ImGui::SameLine();
										if (ImGui::Button("Unlock##CloudDensity")) weather->ClearTarget(WeatherAttribute::CloudDensity);
									}
									float cloud_altitude = atmosphere_effect->GetCloudAltitude();
									if (ImGui::SliderFloat("Cloud Altitude", &cloud_altitude, 0.0f, 1000.0f)) {
										atmosphere_effect->SetCloudAltitude(cloud_altitude);
										changed_atm = true;
									}
									float cloud_thickness = atmosphere_effect->GetCloudThickness() / 1000.0f;
									if (ImGui::SliderFloat("Cloud Thickness", &cloud_thickness, 0.0f, 20.0f)) {
										atmosphere_effect->SetCloudThickness(cloud_thickness * 1000.0f);
										changed_atm = true;
									}
									float cloud_coverage = atmosphere_effect->GetCloudCoverage();
									if (ImGui::SliderFloat("Cloud Coverage", &cloud_coverage, 0.0f, 1.0f)) {
										if (weather) weather->SetTarget(WeatherAttribute::CloudCoverage, cloud_coverage);
										else atmosphere_effect->SetCloudCoverage(cloud_coverage);
										changed_atm = true;
									}
									if (weather) {
										ImGui::SameLine();
										if (ImGui::Button("Unlock##CloudCoverage")) weather->ClearTarget(WeatherAttribute::CloudCoverage);
									}
									float cloud_warp = atmosphere_effect->GetCloudWarp();
									if (ImGui::SliderFloat("Camera Cloud Buffer", &cloud_warp, 0.0f, 1000.0f)) {
										atmosphere_effect->SetCloudWarp(cloud_warp);
										changed_atm = true;
									}
									glm::vec3 cloud_color = atmosphere_effect->GetCloudColor();
									if (ImGui::ColorEdit3("Cloud Color", &cloud_color[0], ImGuiColorEditFlags_HDR|ImGuiColorEditFlags_Float)) {
										if (weather) {
											weather->SetTarget(WeatherAttribute::CloudColorR, cloud_color.r);
											weather->SetTarget(WeatherAttribute::CloudColorG, cloud_color.g);
											weather->SetTarget(WeatherAttribute::CloudColorB, cloud_color.b);
										} else atmosphere_effect->SetCloudColor(cloud_color);
										changed_atm = true;
									}
									if (weather) {
										ImGui::SameLine();
										if (ImGui::Button("Unlock##CloudColor")) {
											weather->ClearTarget(WeatherAttribute::CloudColorR);
											weather->ClearTarget(WeatherAttribute::CloudColorG);
											weather->ClearTarget(WeatherAttribute::CloudColorB);
										}
									}

									auto& cfg = ConfigManager::GetInstance();

									float cloud_shadow = cfg.GetAppSettingFloat("cloud_shadow_intensity", 0.5f);
									if (ImGui::SliderFloat("Cloud Shadow Intensity", &cloud_shadow, 0.0f, 1.0f)) {
										cfg.SetFloat("cloud_shadow_intensity", cloud_shadow);
										changed_atm = true;
									}

									ImGui::Separator();
									ImGui::Text("Advanced Cloud Parameters");

									float g1 = cfg.GetAppSettingFloat(
										"cloud_phase_g1",
										atmosphere_effect->GetCloudPhaseG1()
									);
									if (ImGui::SliderFloat("Phase G1 (Forward)", &g1, 0.0f, 0.99f)) {
										atmosphere_effect->SetCloudPhaseG1(g1);
										cfg.SetFloat("cloud_phase_g1", g1);
										changed_atm = true;
									}
									float g2 = cfg.GetAppSettingFloat(
										"cloud_phase_g2",
										atmosphere_effect->GetCloudPhaseG2()
									);
									if (ImGui::SliderFloat("Phase G2 (Backward)", &g2, -0.99f, 0.0f)) {
										atmosphere_effect->SetCloudPhaseG2(g2);
										cfg.SetFloat("cloud_phase_g2", g2);
										changed_atm = true;
									}
									float alpha = cfg.GetAppSettingFloat(
										"cloud_phase_alpha",
										atmosphere_effect->GetCloudPhaseAlpha()
									);
									if (ImGui::SliderFloat("Phase Mix Alpha", &alpha, 0.0f, 1.0f)) {
										atmosphere_effect->SetCloudPhaseAlpha(alpha);
										cfg.SetFloat("cloud_phase_alpha", alpha);
										changed_atm = true;
									}
									float isotropic = cfg.GetAppSettingFloat(
										"cloud_phase_isotropic",
										atmosphere_effect->GetCloudPhaseIsotropic()
									);
									if (ImGui::SliderFloat("Phase Isotropic Mix", &isotropic, 0.0f, 1.0f)) {
										atmosphere_effect->SetCloudPhaseIsotropic(isotropic);
										cfg.SetFloat("cloud_phase_isotropic", isotropic);
										changed_atm = true;
									}

									float p_scale = cfg.GetAppSettingFloat(
										"cloud_powder_scale",
										atmosphere_effect->GetCloudPowderScale()
									);
									if (ImGui::SliderFloat("Powder Scale", &p_scale, 0.0f, 10.0f)) {
										atmosphere_effect->SetCloudPowderScale(p_scale);
										cfg.SetFloat("cloud_powder_scale", p_scale);
										changed_atm = true;
									}
									float p_mult = cfg.GetAppSettingFloat(
										"cloud_powder_multiplier",
										atmosphere_effect->GetCloudPowderMultiplier()
									);
									if (ImGui::SliderFloat("Powder Multiplier", &p_mult, 0.0f, 100.0f)) {
										atmosphere_effect->SetCloudPowderMultiplier(p_mult);
										cfg.SetFloat("cloud_powder_multiplier", p_mult);
										changed_atm = true;
									}
									float p_local = cfg.GetAppSettingFloat(
										"cloud_powder_local_scale",
										atmosphere_effect->GetCloudPowderLocalScale()
									);
									if (ImGui::SliderFloat("Powder Local Scale", &p_local, 0.0f, 10.0f)) {
										atmosphere_effect->SetCloudPowderLocalScale(p_local);
										cfg.SetFloat("cloud_powder_local_scale", p_local);
										changed_atm = true;
									}

									float s_opt = cfg.GetAppSettingFloat(
										"cloud_shadow_optical_depth_multiplier",
										atmosphere_effect->GetCloudShadowOpticalDepthMultiplier()
									);
									if (ImGui::SliderFloat("Shadow Optical Depth Mult", &s_opt, 0.0f, 10.0f)) {
										atmosphere_effect->SetCloudShadowOpticalDepthMultiplier(s_opt);
										cfg.SetFloat("cloud_shadow_optical_depth_multiplier", s_opt);
										changed_atm = true;
									}
									float s_step = cfg.GetAppSettingFloat(
										"cloud_shadow_step_multiplier",
										atmosphere_effect->GetCloudShadowStepMultiplier()
									);
									if (ImGui::SliderFloat("Shadow Step Mult", &s_step, 0.0f, 10.0f)) {
										atmosphere_effect->SetCloudShadowStepMultiplier(s_step);
										cfg.SetFloat("cloud_shadow_step_multiplier", s_step);
										changed_atm = true;
									}

									float sun_scale = cfg.GetAppSettingFloat(
										"cloud_sun_light_scale",
										atmosphere_effect->GetCloudSunLightScale()
									);
									if (ImGui::SliderFloat("Sun Light Scale", &sun_scale, 0.0f, 50.0f)) {
										atmosphere_effect->SetCloudSunLightScale(sun_scale);
										cfg.SetFloat("cloud_sun_light_scale", sun_scale);
										changed_atm = true;
									}
									float moon_scale = cfg.GetAppSettingFloat(
										"cloud_moon_light_scale",
										atmosphere_effect->GetCloudMoonLightScale()
									);
									if (ImGui::SliderFloat("Moon Light Scale", &moon_scale, 0.0f, 20.0f)) {
										atmosphere_effect->SetCloudMoonLightScale(moon_scale);
										cfg.SetFloat("cloud_moon_light_scale", moon_scale);
										changed_atm = true;
									}

									float bp_mix = cfg.GetAppSettingFloat(
										"cloud_beer_powder_mix",
										atmosphere_effect->GetCloudBeerPowderMix()
									);
									if (ImGui::SliderFloat("Beer-Powder Mix", &bp_mix, 0.0f, 1.0f)) {
										atmosphere_effect->SetCloudBeerPowderMix(bp_mix);
										cfg.SetFloat("cloud_beer_powder_mix", bp_mix);
										changed_atm = true;
									}

									float extinction = atmosphere_effect->GetCloudExtinction();
									if (ImGui::SliderFloat("Extinction Scale", &extinction, 0.001f, 1.0f, "%.3f")) {
										atmosphere_effect->SetCloudExtinction(extinction);
										changed_atm = true;
									}

									glm::vec3 ext_color = atmosphere_effect->GetCloudExtinctionColor();
									if (ImGui::ColorEdit3("Extinction Balance", &ext_color[0], ImGuiColorEditFlags_HDR|ImGuiColorEditFlags_Float)) {
										atmosphere_effect->SetCloudExtinctionColor(ext_color);
										changed_atm = true;
									}

									glm::vec3 albedo = atmosphere_effect->GetCloudAlbedo();
									if (ImGui::ColorEdit3("Cloud Albedo", &albedo[0], ImGuiColorEditFlags_HDR|ImGuiColorEditFlags_Float)) {
										atmosphere_effect->SetCloudAlbedo(albedo);
										changed_atm = true;
									}

									ImGui::Separator();
									ImGui::Text("Cloud Advection & Detail");

									float flow_speed = cfg.GetAppSettingFloat(
										"cloud_flow_speed",
										atmosphere_effect->GetCloudFlowSpeed()
									);
									if (ImGui::SliderFloat("Cloud Flow Speed", &flow_speed, 0.0f, 2.0f)) {
										atmosphere_effect->SetCloudFlowSpeed(flow_speed);
										cfg.SetFloat("cloud_flow_speed", flow_speed);
										changed_atm = true;
									}

									float flow_dir = cfg.GetAppSettingFloat(
										"cloud_flow_direction",
										atmosphere_effect->GetCloudFlowDirection()
									);
									if (ImGui::SliderAngle("Cloud Flow Direction", &flow_dir)) {
										atmosphere_effect->SetCloudFlowDirection(flow_dir);
										cfg.SetFloat("cloud_flow_direction", flow_dir);
										changed_atm = true;
									}

									float flow_height = cfg.GetAppSettingFloat(
										"cloud_flow_height_scale",
										atmosphere_effect->GetCloudFlowHeightScale()
									);
									if (ImGui::SliderFloat("Cloud Flow Height Scale", &flow_height, 0.0f, 0.5f)) {
										atmosphere_effect->SetCloudFlowHeightScale(flow_height);
										cfg.SetFloat("cloud_flow_height_scale", flow_height);
										changed_atm = true;
									}

									float curl_strength = cfg.GetAppSettingFloat(
										"cloud_curl_strength",
										atmosphere_effect->GetCloudCurlStrength()
									);
									if (ImGui::SliderFloat("Cloud Curl Strength", &curl_strength, 0.0f, 20.0f)) {
										atmosphere_effect->SetCloudCurlStrength(curl_strength);
										cfg.SetFloat("cloud_curl_strength", curl_strength);
										changed_atm = true;
									}

									float curl_freq = cfg.GetAppSettingFloat(
										"cloud_curl_frequency",
										atmosphere_effect->GetCloudCurlFrequency()
									);
									if (ImGui::SliderFloat("Cloud Curl Frequency", &curl_freq, 0.1f, 10.0f)) {
										atmosphere_effect->SetCloudCurlFrequency(curl_freq);
										cfg.SetFloat("cloud_curl_frequency", curl_freq);
										changed_atm = true;
									}

									ImGui::Separator();
									ImGui::Text("Scattering");
									float rayleigh = atmosphere_effect->GetRayleighScale();
									if (ImGui::SliderFloat("Rayleigh Scale", &rayleigh, 0.0f, 3.0f)) {
										if (weather) weather->SetTarget(WeatherAttribute::RayleighScale, rayleigh);
										else atmosphere_effect->SetRayleighScale(rayleigh);
										changed_atm = true;
									}
									if (weather) {
										ImGui::SameLine();
										if (ImGui::Button("Unlock##RayleighScale")) weather->ClearTarget(WeatherAttribute::RayleighScale);
									}
									float mie = atmosphere_effect->GetMieScale();
									if (ImGui::SliderFloat("Mie Scale", &mie, 0.0f, 0.25f)) {
										if (weather) weather->SetTarget(WeatherAttribute::MieScale, mie);
										else atmosphere_effect->SetMieScale(mie);
										changed_atm = true;
									}
									if (weather) {
										ImGui::SameLine();
										if (ImGui::Button("Unlock##MieScale")) weather->ClearTarget(WeatherAttribute::MieScale);
									}
									float mie_g = atmosphere_effect->GetMieAnisotropy();
									if (ImGui::SliderFloat("Mie Anisotropy", &mie_g, 0.0f, 0.99f)) {
										atmosphere_effect->SetMieAnisotropy(mie_g);
										changed_atm = true;
									}
									float multi_scat = atmosphere_effect->GetMultiScatScale();
									if (ImGui::SliderFloat("MultiScat Scale", &multi_scat, 0.0f, 0.25f)) {
										atmosphere_effect->SetMultiScatScale(multi_scat);
										changed_atm = true;
									}
									float ambient_scat = atmosphere_effect->GetAmbientScatScale();
									if (ImGui::SliderFloat("Ambient Scat Scale", &ambient_scat, 0.0f, 2.0f)) {
										atmosphere_effect->SetAmbientScatScale(ambient_scat);
										changed_atm = true;
									}

									ImGui::Separator();
									ImGui::Text("Physical Parameters");

									float atmos_height = atmosphere_effect->GetAtmosphereHeight();
									if (ImGui::SliderFloat("Atmosphere Height (km)", &atmos_height, 0.0f, 300.0f)) {
										if (weather) weather->SetTarget(WeatherAttribute::AtmosphereHeight, atmos_height);
										else atmosphere_effect->SetAtmosphereHeight(atmos_height);
										changed_atm = true;
									}
									if (weather) {
										ImGui::SameLine();
										if (ImGui::Button("Unlock##AtmosphereHeight")) weather->ClearTarget(WeatherAttribute::AtmosphereHeight);
									}

									glm::vec3 rayleigh_scattering = atmosphere_effect->GetRayleighScattering() * 1000.0f;
									if (ImGui::ColorEdit3("Rayleigh Scattering", &rayleigh_scattering[0], ImGuiColorEditFlags_HDR|ImGuiColorEditFlags_Float)) {
										if (weather) {
											weather->SetTarget(WeatherAttribute::RayleighScatteringR, rayleigh_scattering.r * 0.001f);
											weather->SetTarget(WeatherAttribute::RayleighScatteringG, rayleigh_scattering.g * 0.001f);
											weather->SetTarget(WeatherAttribute::RayleighScatteringB, rayleigh_scattering.b * 0.001f);
										} else atmosphere_effect->SetRayleighScattering(rayleigh_scattering * 0.001f);
										changed_atm = true;
									}
									if (weather) {
										ImGui::SameLine();
										if (ImGui::Button("Unlock##RayleighScattering")) {
											weather->ClearTarget(WeatherAttribute::RayleighScatteringR);
											weather->ClearTarget(WeatherAttribute::RayleighScatteringG);
											weather->ClearTarget(WeatherAttribute::RayleighScatteringB);
										}
									}

									float mie_scat = atmosphere_effect->GetMieScattering() * 1000.0f;
									if (ImGui::SliderFloat("Mie Scattering coeff", &mie_scat, 0.0f, 10.0f)) {
										if (weather) weather->SetTarget(WeatherAttribute::MieScattering, mie_scat * 0.001f);
										else atmosphere_effect->SetMieScattering(mie_scat * 0.001f);
										changed_atm = true;
									}
									if (weather) {
										ImGui::SameLine();
										if (ImGui::Button("Unlock##MieScattering")) weather->ClearTarget(WeatherAttribute::MieScattering);
									}

									float mie_ext = atmosphere_effect->GetMieExtinction() * 1000.0f;
									if (ImGui::SliderFloat("Mie Extinction coeff", &mie_ext, 0.0f, 10.0f)) {
										if (weather) weather->SetTarget(WeatherAttribute::MieExtinction, mie_ext * 0.001f);
										else atmosphere_effect->SetMieExtinction(mie_ext * 0.001f);
										changed_atm = true;
									}
									if (weather) {
										ImGui::SameLine();
										if (ImGui::Button("Unlock##MieExtinction")) weather->ClearTarget(WeatherAttribute::MieExtinction);
									}

									glm::vec3 ozone_absorption = atmosphere_effect->GetOzoneAbsorption() * 1000.0f;
									if (ImGui::ColorEdit3("Ozone Absorption", &ozone_absorption[0], ImGuiColorEditFlags_HDR|ImGuiColorEditFlags_Float)) {
										atmosphere_effect->SetOzoneAbsorption(ozone_absorption * 0.001f);
										changed_atm = true;
									}

									float rayleigh_h = atmosphere_effect->GetRayleighScaleHeight();
									if (ImGui::SliderFloat("Rayleigh Scale Height (km)", &rayleigh_h, 0.0f, 20.0f)) {
										if (weather) weather->SetTarget(WeatherAttribute::RayleighScaleHeight, rayleigh_h);
										else atmosphere_effect->SetRayleighScaleHeight(rayleigh_h);
										changed_atm = true;
									}
									if (weather) {
										ImGui::SameLine();
										if (ImGui::Button("Unlock##RayleighScaleHeight")) weather->ClearTarget(WeatherAttribute::RayleighScaleHeight);
									}

									float mie_h = atmosphere_effect->GetMieScaleHeight();
									if (ImGui::SliderFloat("Mie Scale Height (km)", &mie_h, 0.0f, 3.0f)) {
										if (weather) weather->SetTarget(WeatherAttribute::MieScaleHeight, mie_h);
										else atmosphere_effect->SetMieScaleHeight(mie_h);
										changed_atm = true;
									}
									if (weather) {
										ImGui::SameLine();
										if (ImGui::Button("Unlock##MieScaleHeight")) weather->ClearTarget(WeatherAttribute::MieScaleHeight);
									}

									ImGui::Separator();
									ImGui::Text("Atmosphere Variance");

									float var_scale = atmosphere_effect->GetColorVarianceScale();
									if (ImGui::SliderFloat("Variance Scale", &var_scale, 0.0f, 2.5f)) {
										atmosphere_effect->SetColorVarianceScale(var_scale);
										changed_atm = true;
									}

									float var_strength = atmosphere_effect->GetColorVarianceStrength();
									if (ImGui::SliderFloat("Variance Strength", &var_strength, 0.0f, 0.5f)) {
										atmosphere_effect->SetColorVarianceStrength(var_strength);
										changed_atm = true;
									}

									ImGui::Separator();
									ImGui::Text("Solar Flares");

									auto& sf_cfg = ConfigManager::GetInstance();
									bool solar_flares = sf_cfg.GetAppSettingBool("solar_flares_enabled", true);
									if (ImGui::Checkbox("Enable Solar Flares", &solar_flares)) {
										sf_cfg.SetBool("solar_flares_enabled", solar_flares);
										changed_atm = true;
									}

									if (solar_flares) {
										float sf_strength = sf_cfg.GetAppSettingFloat("solar_flare_strength", 1.5f);
										if (ImGui::SliderFloat("Flare Strength", &sf_strength, 0.0f, 10.0f)) {
											sf_cfg.SetFloat("solar_flare_strength", sf_strength);
											changed_atm = true;
										}
										float sf_scale = sf_cfg.GetAppSettingFloat("solar_flare_scale", 1.0f);
										if (ImGui::SliderFloat("Flare Scale", &sf_scale, 0.1f, 10.0f)) {
											sf_cfg.SetFloat("solar_flare_scale", sf_scale);
											changed_atm = true;
										}
										float sf_speed = sf_cfg.GetAppSettingFloat("solar_flare_speed", 0.5f);
										if (ImGui::SliderFloat("Flare Speed", &sf_speed, 0.0f, 5.0f)) {
											sf_cfg.SetFloat("solar_flare_speed", sf_speed);
											changed_atm = true;
										}
									}

									ImGui::Separator();
									ImGui::Text("Sky Nebula / Aurora");

									auto& neb_cfg = ConfigManager::GetInstance();
									bool nebula_enabled = neb_cfg.GetAppSettingBool("nebula_enabled", true);
									if (ImGui::Checkbox("Enable Sky Nebula", &nebula_enabled)) {
										neb_cfg.SetBool("nebula_enabled", nebula_enabled);
										changed_atm = true;
									}

									if (nebula_enabled) {
										float neb_intensity = neb_cfg.GetAppSettingFloat("nebula_intensity", 1.0f);
										if (ImGui::SliderFloat("Nebula Intensity", &neb_intensity, 0.0f, 5.0f)) {
											neb_cfg.SetFloat("nebula_intensity", neb_intensity);
											changed_atm = true;
										}
										float neb_threshold = neb_cfg.GetAppSettingFloat("nebula_threshold", 0.0f);
										if (ImGui::SliderFloat("Nebula Noise Threshold", &neb_threshold, 0.0f, 0.9f)) {
											neb_cfg.SetFloat("nebula_threshold", neb_threshold);
											changed_atm = true;
										}
									}

									if (changed_atm) {
										atmosphere_effect->FlushHistory();
									}
								}
							}
							break;
						}
					}
				}

				// 2.5 Volumetric Lighting
				if (ImGui::CollapsingHeader("Volumetric Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
					auto& manager = m_visualizer.GetPostProcessingManager();
					for (auto& effect : manager.GetPreToneMappingEffects()) {
						if (effect->GetName() == "Volumetric Lighting") {
							bool is_enabled = effect->IsEnabled();
							if (ImGui::Checkbox("Enable Volumetric Lighting", &is_enabled)) {
								effect->SetEnabled(is_enabled);
							}

							if (is_enabled) {
								auto vol_effect = std::dynamic_pointer_cast<PostProcessing::VolumetricLightingEffect>(effect);
								if (vol_effect) {
									auto& cfg = ConfigManager::GetInstance();
									bool changed_vol = false;

									float intensity = vol_effect->GetIntensity();
									if (ImGui::SliderFloat("Intensity##Vol", &intensity, 0.0f, 5.0f)) {
										vol_effect->SetIntensity(intensity);
										cfg.SetFloat("volumetric_intensity", intensity);
										changed_vol = true;
									}

									float anisotropy = vol_effect->GetScatteringAnisotropy();
									if (ImGui::SliderFloat("Anisotropy##Vol", &anisotropy, 0.0f, 0.99f)) {
										vol_effect->SetScatteringAnisotropy(anisotropy);
										cfg.SetFloat("volumetric_anisotropy", anisotropy);
										changed_vol = true;
									}

									float alpha = vol_effect->GetTemporalAlpha();
									if (ImGui::SliderFloat("Temporal Alpha##Vol", &alpha, 0.0f, 0.99f)) {
										vol_effect->SetTemporalAlpha(alpha);
										cfg.SetFloat("volumetric_temporal_alpha", alpha);
										changed_vol = true;
									}

									float ambient_scale = vol_effect->GetAmbientScale();
									if (ImGui::SliderFloat("Ambient Scale##Vol", &ambient_scale, 0.0f, 5.0f)) {
										vol_effect->SetAmbientScale(ambient_scale);
										cfg.SetFloat("volumetric_ambient_scale", ambient_scale);
										changed_vol = true;
									}

									float rayleigh_scale = vol_effect->GetRayleighScale();
									if (ImGui::SliderFloat("Rayleigh Scale##Vol", &rayleigh_scale, 0.0f, 5.0f)) {
										vol_effect->SetRayleighScale(rayleigh_scale);
										cfg.SetFloat("volumetric_rayleigh_scale", rayleigh_scale);
										changed_vol = true;
									}

									float mie_scale = vol_effect->GetMieScale();
									if (ImGui::SliderFloat("Mie Scale##Vol", &mie_scale, 0.0f, 5.0f)) {
										vol_effect->SetMieScale(mie_scale);
										cfg.SetFloat("volumetric_mie_scale", mie_scale);
										changed_vol = true;
									}

									float multi_scat_scale = vol_effect->GetMultiScatScale();
									if (ImGui::SliderFloat("Multi-scatter Scale##Vol", &multi_scat_scale, 0.0f, 5.0f)) {
										vol_effect->SetMultiScatScale(multi_scat_scale);
										cfg.SetFloat("volumetric_multi_scat_scale", multi_scat_scale);
										changed_vol = true;
									}

									float shadow_sensitivity = vol_effect->GetShadowSensitivity();
									if (ImGui::SliderFloat("Shadow Sensitivity##Vol", &shadow_sensitivity, 0.0f, 5.0f)) {
										vol_effect->SetShadowSensitivity(shadow_sensitivity);
										cfg.SetFloat("volumetric_shadow_sensitivity", shadow_sensitivity);
										changed_vol = true;
									}

									float light_accent = vol_effect->GetLightAccent();
									if (ImGui::SliderFloat("Light Accent / God Ray Boost##Vol", &light_accent, 1.0f, 10.0f)) {
										vol_effect->SetLightAccent(light_accent);
										cfg.SetFloat("volumetric_light_accent", light_accent);
										changed_vol = true;
									}

									bool use_2d_acc = vol_effect->GetTemporalAccumulation2D();
									if (ImGui::Checkbox("2D Screenspace Temporal Accumulation##Vol", &use_2d_acc)) {
										vol_effect->SetTemporalAccumulation2D(use_2d_acc);
										cfg.SetBool("volumetric_temporal_accumulation_2d", use_2d_acc);
										changed_vol = true;
									}

									if (changed_vol) {
										vol_effect->FlushHistory();
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
						auto& cfg = ConfigManager::GetInstance();
						bool  enabled = decor_manager->IsEnabled();
						if (ImGui::Checkbox("Enable Decor", &enabled)) {
							decor_manager->SetEnabled(enabled);
							cfg.SetBool("render_decor", enabled);
						}

						if (enabled) {
							float threshold = ConfigManager::GetInstance().GetAppSettingFloat(
								"foliage_culling_pixel_threshold",
								10.0f
							);
							if (ImGui::SliderFloat("Decor Pixel Threshold", &threshold, 0.0f, 50.0f)) {
								ConfigManager::GetInstance().SetFloat("foliage_culling_pixel_threshold", threshold);
							}
							ImGui::Text("Higher = cull larger objects, 0 = disable size culling");
						}
					}
				}

				// 4. Particles
				if (ImGui::CollapsingHeader("Particles", ImGuiTreeNodeFlags_DefaultOpen)) {
					auto& config = ConfigManager::GetInstance();

					bool enabled = config.GetAppSettingBool("particles_enabled", true);
					if (ImGui::Checkbox("Enable Particle System", &enabled)) {
						config.SetBool("particles_enabled", enabled);
					}

					if (enabled) {
						bool particle_lights_enabled = config.GetAppSettingBool("particle_lights_enabled", true);
						if (ImGui::Checkbox("Particles Create Lights", &particle_lights_enabled)) {
							config.SetBool("particle_lights_enabled", particle_lights_enabled);
						}

						float density = config.GetAppSettingFloat("ambient_particle_density", 0.15f);
						if (ImGui::SliderFloat("Ambient Density", &density, 0.0f, 2.0f, "%.2f")) {
							config.SetFloat("ambient_particle_density", density);
						}
						ImGui::Text(
							"Approx. %d ambient particles",
							(int)(density * Constants::Class::Particles::AmbientParticleScale())
						);

						ImGui::Separator();
						ImGui::Text("Population Limits & Live Counts");

						auto fire_manager = m_visualizer.GetFireEffectManager();
						if (fire_manager) {
							Boidsish::ParticleStats stats = fire_manager->GetStats();

							auto drawRatioLimit = [&](const char* label, const char* cfg_key, uint32_t current, uint32_t limit, float default_ratio) {
								float ratio = config.GetAppSettingFloat(cfg_key, default_ratio);
								if (ImGui::SliderFloat(label, &ratio, 0.0f, 1.0f, "%.2f")) {
									config.SetFloat(cfg_key, ratio);
								}
								ImGui::SameLine();
								ImGui::Text("(%u/%u live)", current, limit);
							};

							drawRatioLimit("Birds", "particle_ratio_birds", stats.count_birds, stats.limit_birds, 0.05f);
							drawRatioLimit("Butterflies", "particle_ratio_butterflies", stats.count_butterflies, stats.limit_butterflies, 0.15f);
							drawRatioLimit("Leaves", "particle_ratio_leaves", stats.count_leaves, stats.limit_leaves, 0.25f);
							drawRatioLimit("Petals", "particle_ratio_petals", stats.count_petals, stats.limit_petals, 0.25f);
							drawRatioLimit("Bubbles", "particle_ratio_bubbles", stats.count_bubbles, stats.limit_bubbles, 0.15f);
							drawRatioLimit("Fireflies", "particle_ratio_fireflies", stats.count_fireflies, stats.limit_fireflies, 0.25f);
							drawRatioLimit("Fairies", "particle_ratio_fairies", stats.count_fairies, stats.limit_fairies, 0.1f);
							drawRatioLimit("Snow", "particle_ratio_snow", stats.count_snow, stats.limit_snow, 0.5f);
							drawRatioLimit("Rain", "particle_ratio_rain", stats.count_rain, stats.limit_rain, 0.5f);
							drawRatioLimit("Dust", "particle_ratio_dust", stats.count_dust, stats.limit_dust, 0.25f);
						}
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
