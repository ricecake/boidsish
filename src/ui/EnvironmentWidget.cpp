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
#include "state.h"
#include "service_locator.h"
#include "ui/Widgets.h"

namespace Boidsish {
	namespace UI {

		EnvironmentWidget::EnvironmentWidget(Visualizer& visualizer): m_visualizer(visualizer) {}

		void EnvironmentWidget::Draw() {
			if (!m_show)
				return;

			auto store = ServiceLocator::Instance().Get<state::Store>();
			const auto& state = store->GetState();

			ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("Environment", &m_show)) {

				auto drawAttrControl = [&](const char* label, float target, float actual, float min_v, float max_v, const char* fmt, auto action_factory) {
					float val = target;
					ImGui::Text("%s:", label);
					if (SliderFloatWithActual((std::string("##slide") + label).c_str(), &val, actual, min_v, max_v)) {
						store->Dispatch(action_factory(val));
					}
				};

				// 0. Weather
				if (ImGui::CollapsingHeader("Ambient Weather", ImGuiTreeNodeFlags_DefaultOpen)) {
					bool enabled = state.target.weather.enabled;
					if (ImGui::Checkbox("Enable Weather System", &enabled)) {
						store->Dispatch(state::actions::SetWeatherEnabled{enabled});
					}

					if (enabled) {
						float time_scale = state.target.weather.timeScale;
						if (ImGui::SliderFloat("Weather Time Scale", &time_scale, 0.0f, 0.015f, "%.3f")) {
							store->Dispatch(state::actions::SetWeatherTimeScale{time_scale});
						}

						float spatial_scale = state.target.weather.spatialScale;
						if (ImGui::SliderFloat("Weather Spatial Scale", &spatial_scale, 0.0f, 0.003f, "%.4f")) {
							store->Dispatch(state::actions::SetWeatherSpatialScale{spatial_scale});
						}

						float hold_threshold = state.target.weather.holdThreshold;
						if (ImGui::SliderFloat("Hold Threshold", &hold_threshold, 0.0f, 1.0f, "%.2f")) {
							store->Dispatch(state::actions::SetWeatherHoldThreshold{hold_threshold});
						}

						ImGui::Separator();

						drawAttrControl("Temperature (K)", state.target.weather.temperature, state.actual.weather.temperature, 200.0f, 350.0f, "%.1f K", [](float v){ return state::actions::SetWeatherTemperature{v}; });
						float temp = state.actual.weather.temperature;
						ImGui::Text("  (%.1f C / %.1f F)", temp - 273.15f, (temp - 273.15f) * 1.8f + 32.0f);

						drawAttrControl("Precipitation", state.target.weather.precipitation, state.actual.weather.precipitation, 0.0f, 1.0f, "%.2f", [](float v){ return state::actions::SetWeatherPrecipitation{v}; });
						drawAttrControl("Humidity", state.target.weather.humidity, state.actual.weather.humidity, 0.0f, 1.0f, "%.2f", [](float v){ return state::actions::SetWeatherHumidity{v}; });
						drawAttrControl("Wind Strength", state.target.weather.windStrength, state.actual.weather.windStrength, 0.0f, 5.0f, "%.2f", [](float v){ return state::actions::SetWeatherWindStrength{v}; });
						drawAttrControl("Cloud Coverage", state.target.weather.cloudCoverage, state.actual.weather.cloudCoverage, 0.0f, 1.0f, "%.2f", [](float v){ return state::actions::SetWeatherCloudCoverage{v}; });

						// Display derived intensities and wetness
						float rain = (state.actual.weather.temperature > 273.15f) ? state.actual.weather.precipitation : 0.0f;
						float snow = (state.actual.weather.temperature <= 273.15f) ? state.actual.weather.precipitation : 0.0f;
						ImGui::Text("Rain Intensity: %.2f", rain);
						ImGui::Text("Snow Intensity: %.2f", snow);

						ImGui::Separator();

						if (ImGui::TreeNode("Macro Weather Simulation")) {
							bool macro_enabled = state.target.weather.macroSimEnabled;
							if (ImGui::Checkbox("Enable Macro Simulation", &macro_enabled)) {
								store->Dispatch(state::actions::SetWeatherMacroSimEnabled{macro_enabled});
							}

							if (macro_enabled) {
								float tau = state.target.weather.simTau;
								if (ImGui::SliderFloat("Simulation Relaxation (Tau)", &tau, 0.51f, 2.0f, "%.2f")) {
									store->Dispatch(state::actions::SetWeatherSimTau{tau});
								}

								bool strict = state.target.weather.strictEnforcement;
								if (ImGui::Checkbox("Strict Enforcement (Instant Snap)", &strict)) {
									store->Dispatch(state::actions::SetWeatherStrictEnforcement{strict});
								}

								float stiffness = state.target.weather.nudgeStiffness;
								if (ImGui::SliderFloat("Nudge Stiffness", &stiffness, 0.01f, 10.0f, "%.2f")) {
									store->Dispatch(state::actions::SetWeatherNudgeStiffness{stiffness});
								}

								ImGui::Separator();
								ImGui::Text("Simulation Constraints");

								auto drawConstraint = [&](const char* label, const char* type, const state::LbmConstraint& con, float min_v, float max_v) {
									if (ImGui::TreeNode(label)) {
										bool has_min = con.min.has_value();
										float min_val = con.min.value_or(min_v);
										if (ImGui::Checkbox("Min", &has_min)) {
											store->Dispatch(state::actions::SetWeatherLbmConstraint{type, has_min ? std::optional<float>(min_val) : std::nullopt, con.max, con.target});
										}
										if (has_min) {
											ImGui::SameLine();
											if (ImGui::SliderFloat("##min", &min_val, min_v, max_v)) {
												store->Dispatch(state::actions::SetWeatherLbmConstraint{type, min_val, con.max, con.target});
											}
										}

										bool has_max = con.max.has_value();
										float max_val = con.max.value_or(max_v);
										if (ImGui::Checkbox("Max", &has_max)) {
											store->Dispatch(state::actions::SetWeatherLbmConstraint{type, con.min, has_max ? std::optional<float>(max_val) : std::nullopt, con.target});
										}
										if (has_max) {
											ImGui::SameLine();
											if (ImGui::SliderFloat("##max", &max_val, min_v, max_v)) {
												store->Dispatch(state::actions::SetWeatherLbmConstraint{type, con.min, max_val, con.target});
											}
										}

										bool has_target = con.target.has_value();
										float target_val = con.target.value_or((min_v + max_v) * 0.5f);
										if (ImGui::Checkbox("Target", &has_target)) {
											store->Dispatch(state::actions::SetWeatherLbmConstraint{type, con.min, con.max, has_target ? std::optional<float>(target_val) : std::nullopt});
										}
										if (has_target) {
											ImGui::SameLine();
											if (ImGui::SliderFloat("##target", &target_val, min_v, max_v)) {
												store->Dispatch(state::actions::SetWeatherLbmConstraint{type, con.min, con.max, target_val});
											}
										}
										ImGui::TreePop();
									}
								};

								drawConstraint("Temperature", "temperature", state.target.weather.constraints.temperature, 200.0f, 350.0f);
								drawConstraint("Pressure", "pressure", state.target.weather.constraints.pressure, 900.0f, 1100.0f);
								drawConstraint("Humidity", "humidity", state.target.weather.constraints.humidity, 0.0f, 1.0f);
								drawConstraint("Wind Velocity", "velocity", state.target.weather.constraints.velocity, 0.0f, 50.0f);
								drawConstraint("Aerosols", "aerosols", state.target.weather.constraints.aerosols, 0.0f, 1.0f);

								ImGui::Separator();
								ImGui::Text("Atmospheric Injections");
								static glm::vec3 injPos(0.0f);
								ImGui::DragFloat3("Position", &injPos[0]);

								if (ImGui::Button("Inject Pressure Burst")) {
									store->Dispatch(state::actions::InjectPressure{injPos, 1050.0f, 5.0f});
								}
								ImGui::SameLine();
								if (ImGui::Button("Inject Aerosol Cloud")) {
									store->Dispatch(state::actions::InjectAerosol{injPos, 0.8f});
								}
								if (ImGui::Button("Inject Heat Source")) {
									store->Dispatch(state::actions::InjectTemperature{injPos, 320.0f});
								}
							}
							ImGui::TreePop();
						}
					}
				}

				// 3.5 Grass
				if (ImGui::CollapsingHeader("Grass", ImGuiTreeNodeFlags_DefaultOpen)) {
					bool enabled = state.target.grass.enabled;
					if (ImGui::Checkbox("Enable Grass", &enabled)) {
						store->Dispatch(state::actions::SetGrassEnabled{enabled});
					}

					if (enabled) {
						float length = state.target.grass.lengthMultiplier;
						if (ImGui::SliderFloat("Length Multiplier", &length, 0.1f, 5.0f)) {
							store->Dispatch(state::actions::SetGrassLength{length});
						}
						float width = state.target.grass.widthMultiplier;
						if (ImGui::SliderFloat("Width Multiplier", &width, 0.1f, 5.0f)) {
							store->Dispatch(state::actions::SetGrassWidth{width});
						}
						float density = state.target.grass.densityMultiplier;
						if (ImGui::SliderFloat("Density Multiplier", &density, 0.0f, 2.0f)) {
							store->Dispatch(state::actions::SetGrassDensity{density});
						}
						float rigidity = state.target.grass.rigidityMultiplier;
						if (ImGui::SliderFloat("Rigidity Multiplier", &rigidity, 0.0f, 2.0f)) {
							store->Dispatch(state::actions::SetGrassRigidity{rigidity});
						}
						float wind = state.target.grass.windMultiplier;
						if (ImGui::SliderFloat("Wind Multiplier", &wind, 0.0f, 5.0f)) {
							store->Dispatch(state::actions::SetGrassWind{wind});
						}
					}
				}

				// 1. Day/Night Cycle
				if (ImGui::CollapsingHeader("Day/Night Cycle", ImGuiTreeNodeFlags_DefaultOpen)) {
					bool dn_enabled = state.target.dayNight.enabled;
					if (ImGui::Checkbox("Enabled##DN", &dn_enabled)) {
						store->Dispatch(state::actions::SetDayNightEnabled{dn_enabled});
					}
					float dn_time = state.target.dayNight.time;
					if (ImGui::SliderFloat("Time (24h)", &dn_time, 0.0f, 24.0f, "%.1f h")) {
						store->Dispatch(state::actions::SetDayNightTime{dn_time});
					}
					float dn_speed = state.target.dayNight.speed;
					if (ImGui::SliderFloat("Speed", &dn_speed, 0.0f, 2.0f, "%.2f")) {
						store->Dispatch(state::actions::SetDayNightSpeed{dn_speed});
					}
					bool dn_paused = state.target.dayNight.paused;
					if (ImGui::Checkbox("Paused", &dn_paused)) {
						store->Dispatch(state::actions::SetDayNightPaused{dn_paused});
					}

					ImGui::Separator();
					ImGui::Text("Moon");
					float albedo = state.target.dayNight.lunarAlbedo;
					if (ImGui::SliderFloat("Lunar Albedo", &albedo, 0.0f, 0.5f, "%.3f")) {
						store->Dispatch(state::actions::SetLunarAlbedo{albedo});
					}
					glm::vec3 tint = state.target.dayNight.moonTint;
					if (ImGui::ColorEdit3("Moon Tint", &tint[0])) {
						store->Dispatch(state::actions::SetMoonTint{tint});
					}
					float month = state.target.dayNight.lunarMonth;
					if (ImGui::SliderFloat("Lunar Month (days)", &month, 0.1f, 30.0f, "%.2f")) {
						store->Dispatch(state::actions::SetLunarMonth{month});
					}
					float phase = state.target.dayNight.moonPhaseDays;
					if (ImGui::SliderFloat("Moon Phase (days)", &phase, 0.0f, month, "%.1f")) {
						store->Dispatch(state::actions::SetMoonPhaseDays{phase});
					}
				}

				// 2. Atmosphere
				if (ImGui::CollapsingHeader("Atmosphere", ImGuiTreeNodeFlags_DefaultOpen)) {
					bool at_enabled = state.target.atmosphere.enabled;
					if (ImGui::Checkbox("Enable Atmosphere", &at_enabled)) {
						store->Dispatch(state::actions::SetAtmosphereEnabled{at_enabled});
					}

					if (at_enabled) {
						drawAttrControl("Haze Density", state.target.atmosphere.hazeDensity, state.actual.atmosphere.hazeDensity, 0.0f, 5.0f, "%.2f", [](float v){ return state::actions::SetHazeDensity{v}; });
						drawAttrControl("Haze Height", state.target.atmosphere.hazeHeight, state.actual.atmosphere.hazeHeight, 0.0f, 50.0f, "%.1f", [](float v){ return state::actions::SetHazeHeight{v}; });

						glm::vec3 haze_color = state.target.atmosphere.hazeColor;
						if (ImGui::ColorEdit3("Haze Color", &haze_color[0])) {
							store->Dispatch(state::actions::SetHazeColor{haze_color});
						}

						drawAttrControl("Cloud Density", state.target.atmosphere.cloudDensity, state.actual.atmosphere.cloudDensity, 0.0f, 1.0f, "%.2f", [](float v){ return state::actions::SetCloudDensity{v}; });
						drawAttrControl("Cloud Altitude", state.target.atmosphere.cloudAltitude, state.actual.atmosphere.cloudAltitude, 0.0f, 1000.0f, "%.0f", [](float v){ return state::actions::SetCloudAltitude{v}; });
						drawAttrControl("Cloud Thickness", state.target.atmosphere.cloudThickness, state.actual.atmosphere.cloudThickness, 0.0f, 500.0f, "%.0f", [](float v){ return state::actions::SetCloudThickness{v}; });
						drawAttrControl("Cloud Coverage", state.target.atmosphere.cloudCoverage, state.actual.atmosphere.cloudCoverage, 0.0f, 2.0f, "%.2f", [](float v){ return state::actions::SetCloudCoverage{v}; });

						float cloud_warp = state.target.atmosphere.cloudWarp;
						if (ImGui::SliderFloat("Camera Cloud Buffer", &cloud_warp, 0.0f, 1000.0f)) {
							store->Dispatch(state::actions::SetCloudWarp{cloud_warp});
						}

						glm::vec3 cloud_color = state.target.atmosphere.cloudColor;
						if (ImGui::ColorEdit3("Cloud Color", &cloud_color[0])) {
							store->Dispatch(state::actions::SetCloudColor{cloud_color});
						}

						ImGui::Separator();
						ImGui::Text("Advanced Cloud Parameters");
						float cloudSun = state.target.atmosphere.cloudSunLightScale;
						if (ImGui::SliderFloat("Sun Light Scale", &cloudSun, 0.0f, 50.0f)) {
							store->Dispatch(state::actions::SetCloudSunLightScale{cloudSun});
						}
						float cloudMoon = state.target.atmosphere.cloudMoonLightScale;
						if (ImGui::SliderFloat("Moon Light Scale", &cloudMoon, 0.0f, 20.0f)) {
							store->Dispatch(state::actions::SetCloudMoonLightScale{cloudMoon});
						}
						float cloudPowder = state.target.atmosphere.cloudPowderScale;
						if (ImGui::SliderFloat("Powder Scale", &cloudPowder, 0.0f, 2.0f)) {
							store->Dispatch(state::actions::SetCloudPowderScale{cloudPowder});
						}
						float cloudBeer = state.target.atmosphere.cloudBeerPowderMix;
						if (ImGui::SliderFloat("Beer-Powder Mix", &cloudBeer, 0.0f, 1.0f)) {
							store->Dispatch(state::actions::SetCloudBeerPowderMix{cloudBeer});
						}

						ImGui::Separator();
						ImGui::Text("Scattering");
						drawAttrControl("Rayleigh Scale", state.target.atmosphere.rayleighScale, state.actual.atmosphere.rayleighScale, 0.0f, 3.0f, "%.2f", [](float v){ return state::actions::SetRayleighScale{v}; });
						drawAttrControl("Mie Scale", state.target.atmosphere.mieScale, state.actual.atmosphere.mieScale, 0.0f, 0.25f, "%.2f", [](float v){ return state::actions::SetMieScale{v}; });

						if (ImGui::TreeNode("Physical Parameters")) {
							drawAttrControl("Atmosphere Height (km)", state.target.atmosphere.atmosphereHeight, state.actual.atmosphere.atmosphereHeight, 0.0f, 300.0f, "%.0f", [](float v){ return state::actions::SetAtmosphereHeight{v}; });
							drawAttrControl("Rayleigh Scale Height", state.target.atmosphere.rayleighScaleHeight, state.actual.atmosphere.rayleighScaleHeight, 1.0f, 20.0f, "%.1f", [](float v){ return state::actions::SetRayleighScaleHeight{v}; });
							drawAttrControl("Mie Scale Height", state.target.atmosphere.mieScaleHeight, state.actual.atmosphere.mieScaleHeight, 0.1f, 10.0f, "%.1f", [](float v){ return state::actions::SetMieScaleHeight{v}; });

							glm::vec3 ozone = state.target.atmosphere.ozoneAbsorption;
							if (ImGui::ColorEdit3("Ozone Absorption", &ozone[0])) {
								store->Dispatch(state::actions::SetOzoneAbsorption{ozone});
							}

							glm::vec3 rayleigh = state.target.atmosphere.rayleighScattering;
							if (ImGui::ColorEdit3("Rayleigh Scattering", &rayleigh[0])) {
								store->Dispatch(state::actions::SetRayleighScattering{rayleigh});
							}

							drawAttrControl("Mie Scattering", state.target.atmosphere.mieScattering, state.actual.atmosphere.mieScattering, 0.0f, 0.1f, "%.4f", [](float v){ return state::actions::SetMieScattering{v}; });
							drawAttrControl("Mie Extinction", state.target.atmosphere.mieExtinction, state.actual.atmosphere.mieExtinction, 0.0f, 0.1f, "%.4f", [](float v){ return state::actions::SetMieExtinction{v}; });

							ImGui::TreePop();
						}
					}
				}

				// 2.5 Volumetric Lighting
				if (ImGui::CollapsingHeader("Volumetric Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
					bool vol_enabled = state.target.volumetric.enabled;
					if (ImGui::Checkbox("Enable Volumetric Lighting", &vol_enabled)) {
						store->Dispatch(state::actions::SetVolumetricEnabled{vol_enabled});
					}

					if (vol_enabled) {
						float intensity = state.target.volumetric.intensity;
						if (ImGui::SliderFloat("Intensity##Vol", &intensity, 0.0f, 5.0f)) {
							store->Dispatch(state::actions::SetVolumetricIntensity{intensity});
						}
						float anisotropy = state.target.volumetric.anisotropy;
						if (ImGui::SliderFloat("Anisotropy##Vol", &anisotropy, 0.0f, 0.99f)) {
							store->Dispatch(state::actions::SetVolumetricAnisotropy{anisotropy});
						}
						float alpha = state.target.volumetric.temporalAlpha;
						if (ImGui::SliderFloat("Temporal Alpha##Vol", &alpha, 0.0f, 0.99f)) {
							store->Dispatch(state::actions::SetVolumetricTemporalAlpha{alpha});
						}
					}
				}

				// 3. Terrain
				if (ImGui::CollapsingHeader("Terrain", ImGuiTreeNodeFlags_DefaultOpen)) {
					bool render_terrain = state.target.terrain.renderTerrain;
					if (ImGui::Checkbox("Render Terrain", &render_terrain)) {
						store->Dispatch(state::actions::SetRenderTerrain{render_terrain});
					}
					bool render_floor = state.target.terrain.renderFloor;
					if (ImGui::Checkbox("Render Floor", &render_floor)) {
						store->Dispatch(state::actions::SetRenderFloor{render_floor});
					}
					bool force_both = state.target.terrain.forceBoth;
					if (ImGui::Checkbox("Force Both Floor and Terrain", &force_both)) {
						store->Dispatch(state::actions::SetForceBoth{force_both});
					}

					ImGui::Separator();

					float world_scale = state.target.terrain.worldScale;
					if (ImGui::SliderFloat("World Scale", &world_scale, 0.1f, 5.0f)) {
						store->Dispatch(state::actions::SetWorldScale{world_scale});
					}
					ImGui::Text("Higher = larger world, Lower = smaller world");

					bool fol_enabled = state.target.terrain.foliageEnabled;
					if (ImGui::Checkbox("Enable Foliage", &fol_enabled)) {
						store->Dispatch(state::actions::SetFoliageEnabled{fol_enabled});
					}

					if (fol_enabled) {
						float threshold = state.target.terrain.foliagePixelThreshold;
						if (ImGui::SliderFloat("Foliage Pixel Threshold", &threshold, 0.0f, 50.0f)) {
							store->Dispatch(state::actions::SetFoliagePixelThreshold{threshold});
						}
					}
				}

				// 4. Particles
				if (ImGui::CollapsingHeader("Particles", ImGuiTreeNodeFlags_DefaultOpen)) {
					bool part_enabled = state.target.particles.enabled;
					if (ImGui::Checkbox("Enable Particle System", &part_enabled)) {
						store->Dispatch(state::actions::SetParticlesEnabled{part_enabled});
					}

					if (part_enabled) {
						float density = state.target.particles.ambientDensity;
						if (ImGui::SliderFloat("Ambient Density", &density, 0.0f, 2.0f, "%.2f")) {
							store->Dispatch(state::actions::SetAmbientParticleDensity{density});
						}

						auto drawLimit = [&](const char* label, const char* type, uint32_t current, uint32_t limit) {
							int i_limit = (int)limit;
							if (ImGui::SliderInt(label, &i_limit, 0, 20000)) {
								store->Dispatch(state::actions::SetParticleLimit{type, (uint32_t)i_limit});
							}
							ImGui::SameLine();
							ImGui::Text("(%u live)", current);
						};

						drawLimit("Birds", "birds", state.actual.particles.countBirds, state.actual.particles.limitBirds);
						drawLimit("Leaves", "leaves", state.actual.particles.countLeaves, state.actual.particles.limitLeaves);
						drawLimit("Petals", "petals", state.actual.particles.countPetals, state.actual.particles.limitPetals);
						drawLimit("Bubbles", "bubbles", state.actual.particles.countBubbles, state.actual.particles.limitBubbles);
						drawLimit("Fireflies", "fireflies", state.actual.particles.countFireflies, state.actual.particles.limitFireflies);
						drawLimit("Snow", "snow", state.actual.particles.countSnow, state.actual.particles.limitSnow);
						drawLimit("Rain", "rain", state.actual.particles.countRain, state.actual.particles.limitRain);
						drawLimit("Dust", "dust", state.actual.particles.countDust, state.actual.particles.limitDust);
					}
				}

				// 6. Terrain Erosion
				if (ImGui::CollapsingHeader("Terrain Erosion", ImGuiTreeNodeFlags_DefaultOpen)) {
					bool erosion_enabled = state.target.erosion.enabled;
					if (ImGui::Checkbox("Enable Erosion Filter", &erosion_enabled)) {
						store->Dispatch(state::actions::SetErosionEnabled{erosion_enabled});
					}

					if (erosion_enabled) {
						float strength = state.target.erosion.strength;
						if (ImGui::SliderFloat("Erosion Strength", &strength, 0.0f, 0.5f, "%.3f")) {
							store->Dispatch(state::actions::SetErosionStrength{strength});
						}
						float scale = state.target.erosion.scale;
						if (ImGui::SliderFloat("Erosion Scale", &scale, 0.01f, 1.0f, "%.2f")) {
							store->Dispatch(state::actions::SetErosionScale{scale});
						}
						float detail = state.target.erosion.detail;
						if (ImGui::SliderFloat("Erosion Detail", &detail, 0.1f, 5.0f, "%.1f")) {
							store->Dispatch(state::actions::SetErosionDetail{detail});
						}
						float gully = state.target.erosion.gullyWeight;
						if (ImGui::SliderFloat("Gully Weight", &gully, 0.0f, 1.0f, "%.2f")) {
							store->Dispatch(state::actions::SetErosionGullyWeight{gully});
						}
						float dist = state.target.erosion.maxDist;
						if (ImGui::SliderFloat("Erosion Max Dist", &dist, 50.0f, 1000.0f, "%.0f")) {
							store->Dispatch(state::actions::SetErosionMaxDist{dist});
						}
					}
				}
			}
			ImGui::End();
		}
	} // namespace UI
} // namespace Boidsish
