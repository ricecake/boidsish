#include "ui/SystemWidget.h"

#include <ctime>
#include <iomanip>
#include <sstream>

#include "ConfigManager.h"
#include "SceneManager.h"
#include "graphics.h"
#include "imgui.h"
#include "post_processing/PostProcessingManager.h"
#include "post_processing/effects/AtmosphereEffect.h"
#include "post_processing/effects/BloomEffect.h"
#include "post_processing/effects/FilmGrainEffect.h"

namespace Boidsish {
	namespace UI {

		SystemWidget::SystemWidget(Visualizer& visualizer, SceneManager& sceneManager):
			m_visualizer(visualizer), m_sceneManager(sceneManager) {
			m_saveName[0] = '\0';
			m_saveCamera = true;
			m_moveCamera = true;
			m_saveEffects = true;
			m_applyEffects = true;
			m_newDictName[0] = '\0';
		}

		void RenderSection(const std::string& section_name, bool is_global) {
			if (ImGui::CollapsingHeader(section_name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
				auto& config_manager = ConfigManager::GetInstance();
				auto  registered_values = config_manager.GetRegisteredValues(section_name);

				for (const auto& pair : registered_values) {
					const std::string& key = pair.first;
					const ConfigValue& val_info = pair.second;

					// Filter out keys that are now handled by specific widgets
					if (key.contains("artistic_effect_") || key == "render_scale" || key == "enable_shadows" ||
					    key.contains("mesh_simplifier_") || key == "mesh_optimizer_enabled" ||
					    key.contains("wind_") || key.contains("camera_") || key == "enable_foliage") {
						continue;
					}

					switch (val_info.type) {
					case ConfigValue::Type::BOOL: {
						bool value = is_global ? config_manager.GetGlobalSettingBool(key, val_info.bool_value)
											   : config_manager.GetAppSettingBool(key, val_info.bool_value);
						if (ImGui::Checkbox(key.c_str(), &value)) {
							config_manager.SetBool(key, value);
						}
						break;
					}
					case ConfigValue::Type::INT: {
						int value = is_global ? config_manager.GetGlobalSettingInt(key, val_info.int_value)
											  : config_manager.GetAppSettingInt(key, val_info.int_value);
						if (ImGui::InputInt(key.c_str(), &value)) {
							config_manager.SetInt(key, value);
						}
						break;
					}
					case ConfigValue::Type::FLOAT: {
						float value = is_global ? config_manager.GetGlobalSettingFloat(key, val_info.float_value)
												: config_manager.GetAppSettingFloat(key, val_info.float_value);
						if (ImGui::InputFloat(key.c_str(), &value)) {
							config_manager.SetFloat(key, value);
						}
						break;
					}
					case ConfigValue::Type::STRING: {
						char        buffer[256];
						std::string current_val = is_global
							? config_manager.GetGlobalSettingString(key, val_info.string_value)
							: config_manager.GetAppSettingString(key, val_info.string_value);
						strncpy(buffer, current_val.c_str(), sizeof(buffer) - 1);
						buffer[sizeof(buffer) - 1] = '\0';

						if (ImGui::InputText(key.c_str(), buffer, sizeof(buffer))) {
							config_manager.SetString(key, std::string(buffer));
						}
						break;
					}
					}
				}
			}
		}

		void SystemWidget::Draw() {
			if (!m_show)
				return;

			ImGui::SetNextWindowPos(ImVec2(540, 640), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("System", &m_show)) {
				// 1. Camera & Follow Camera (from ConfigWidget)
				if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
					float camera_speed = m_visualizer.GetCamera().speed;
					if (ImGui::SliderFloat("Speed", &camera_speed, 0.5f, 100.0f)) {
						Camera cam = m_visualizer.GetCamera();
						cam.speed = camera_speed;
						m_visualizer.SetCamera(cam);
					}

					const char* modes[] = {"Free", "Auto", "Tracking", "Stationary", "Chase", "Path Follow"};
					int         current_mode = static_cast<int>(m_visualizer.GetCameraMode());
					if (ImGui::Combo("Mode", &current_mode, modes, IM_ARRAYSIZE(modes))) {
						m_visualizer.SetCameraMode(static_cast<CameraMode>(current_mode));
					}

					if (ImGui::Button("Next Chase Target")) {
						m_visualizer.CycleChaseTarget();
					}

					ImGui::Separator();
					ImGui::Text("Follow Settings");
					Camera& cam = m_visualizer.GetCamera();
					bool    changed = false;

					if (ImGui::SliderFloat("Trail Distance", &cam.follow_distance, 0.0f, 50.0f))
						changed = true;
					if (ImGui::SliderFloat("Elevation", &cam.follow_elevation, -20.0f, 20.0f))
						changed = true;
					if (ImGui::SliderFloat("Look Ahead", &cam.follow_look_ahead, 0.0f, 50.0f))
						changed = true;
					if (ImGui::SliderFloat("Responsiveness", &cam.follow_responsiveness, 0.1f, 20.0f))
						changed = true;

					ImGui::Separator();
					ImGui::Text("Path Following");
					if (ImGui::SliderFloat("Path Smoothing", &cam.path_smoothing, 0.1f, 20.0f))
						changed = true;
					if (ImGui::SliderFloat("Bank Factor", &cam.path_bank_factor, 0.0f, 5.0f))
						changed = true;
					if (ImGui::SliderFloat("Bank Speed", &cam.path_bank_speed, 0.1f, 20.0f))
						changed = true;

					if (changed) {
						m_visualizer.SetCamera(cam);
					}
				}

				// 2. Time (from ConfigWidget)
				if (ImGui::CollapsingHeader("Time", ImGuiTreeNodeFlags_DefaultOpen)) {
					float time_scale = m_visualizer.GetTimeScale();
					if (ImGui::SliderFloat("Time Scale", &time_scale, 0.01f, 5.0f)) {
						m_visualizer.SetTimeScale(time_scale);
					}

					bool paused = m_visualizer.GetPause();
					if (ImGui::Checkbox("Paused", &paused)) {
						m_visualizer.SetPause(paused);
					}
				}

				// 3. Scene Management (from SceneWidget)
				if (ImGui::CollapsingHeader("Scene Manager", ImGuiTreeNodeFlags_DefaultOpen)) {
					auto        dictionaries = m_sceneManager.GetDictionaries();
					std::string currentDict = m_sceneManager.GetCurrentDictionaryName();

					if (ImGui::BeginCombo("Dictionary", currentDict.empty() ? "Select..." : currentDict.c_str())) {
						for (const auto& dict : dictionaries) {
							bool isSelected = (currentDict == dict);
							if (ImGui::Selectable(dict.c_str(), isSelected)) {
								m_sceneManager.LoadDictionary(dict);
							}
							if (isSelected)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}

					ImGui::InputText("New Dictionary", m_newDictName, sizeof(m_newDictName));
					ImGui::SameLine();
					if (ImGui::Button("Create")) {
						m_sceneManager.SaveDictionary(m_newDictName);
						m_newDictName[0] = '\0';
					}

					ImGui::Separator();

					if (!currentDict.empty()) {
						ImGui::Text("Save Current Scene");
						ImGui::InputText("Scene Name", m_saveName, sizeof(m_saveName));
						ImGui::Checkbox("Save Camera", &m_saveCamera);
						ImGui::Checkbox("Save Effects", &m_saveEffects);
						if (ImGui::Button("Save")) {
							Scene scene;
							scene.name = m_saveName;
							scene.timestamp = (long long)std::time(nullptr);
							scene.lights = m_visualizer.GetLightManager().GetLights();
							scene.ambient_light = m_visualizer.GetLightManager().GetAmbientLight();
							if (m_saveCamera) {
								scene.camera = m_visualizer.GetCamera();
							}

							if (m_saveEffects) {
								scene.object_effects.ripple = m_visualizer.IsRippleEffectEnabled();
								scene.object_effects.color_shift = m_visualizer.IsColorShiftEffectEnabled();
								scene.object_effects.black_and_white = m_visualizer.IsBlackAndWhiteEffectEnabled();
								scene.object_effects.negative = m_visualizer.IsNegativeEffectEnabled();
								scene.object_effects.shimmery = m_visualizer.IsShimmeryEffectEnabled();
								scene.object_effects.glitched = m_visualizer.IsGlitchedEffectEnabled();
								scene.object_effects.wireframe = m_visualizer.IsWireframeEffectEnabled();

								auto& ppm = m_visualizer.GetPostProcessingManager();
								for (auto& effect : ppm.GetPreToneMappingEffects()) {
									if (auto bloom = std::dynamic_pointer_cast<PostProcessing::BloomEffect>(effect)) {
										scene.post_processing.bloom_enabled = bloom->IsEnabled();
										scene.post_processing.bloom_intensity = bloom->GetIntensity();
										scene.post_processing.bloom_threshold = bloom->GetThreshold();
									} else if (auto atmos =
												   std::dynamic_pointer_cast<PostProcessing::AtmosphereEffect>(effect)) {
										scene.post_processing.atmosphere_enabled = atmos->IsEnabled();
										scene.post_processing.haze_density = atmos->GetHazeDensity();
										scene.post_processing.haze_height = atmos->GetHazeHeight();
										scene.post_processing.haze_color = atmos->GetHazeColor();
										scene.post_processing.cloud_density = atmos->GetCloudDensity();
										scene.post_processing.cloud_altitude = atmos->GetCloudAltitude();
										scene.post_processing.cloud_thickness = atmos->GetCloudThickness();
										scene.post_processing.cloud_color = atmos->GetCloudColor();
									} else if (auto fg = std::dynamic_pointer_cast<PostProcessing::FilmGrainEffect>(
												   effect
											   )) {
										scene.post_processing.film_grain_enabled = fg->IsEnabled();
										scene.post_processing.film_grain_intensity = fg->GetIntensity();
									} else if (effect->GetName() == "Negative") {
										scene.post_processing.negative_enabled = effect->IsEnabled();
									} else if (effect->GetName() == "Glitch") {
										scene.post_processing.glitch_enabled = effect->IsEnabled();
									} else if (effect->GetName() == "OpticalFlow") {
										scene.post_processing.optical_flow_enabled = effect->IsEnabled();
									} else if (effect->GetName() == "Strobe") {
										scene.post_processing.strobe_enabled = effect->IsEnabled();
									} else if (effect->GetName() == "WhispTrail") {
										scene.post_processing.whisp_trail_enabled = effect->IsEnabled();
									} else if (effect->GetName() == "TimeStutter") {
										scene.post_processing.time_stutter_enabled = effect->IsEnabled();
									}
								}
								if (auto tm = ppm.GetToneMappingEffect()) {
									scene.post_processing.tone_mapping_enabled = tm->IsEnabled();
								}
							}

							m_sceneManager.AddScene(scene);
							m_sceneManager.SaveDictionary(currentDict);
							m_saveName[0] = '\0';
						}

						ImGui::Separator();

						ImGui::Text("Load Scene");
						auto&      scenes = m_sceneManager.GetScenes();
						static int selectedScene = -1;

						std::string preview = "Select Scene...";
						if (selectedScene >= 0 && selectedScene < (int)scenes.size()) {
							if (!scenes[selectedScene].name.empty()) {
								preview = scenes[selectedScene].name;
							} else {
								std::time_t       t = (std::time_t)scenes[selectedScene].timestamp;
								std::tm*          tm = std::localtime(&t);
								std::stringstream ss;
								ss << std::put_time(tm, "%Y-%m-%d %H:%M:%S");
								preview = ss.str();
							}
						}

						if (ImGui::BeginCombo("Scenes", preview.c_str())) {
							for (int i = 0; i < (int)scenes.size(); ++i) {
								std::string label;
								if (!scenes[i].name.empty()) {
									label = scenes[i].name;
								} else {
									std::time_t       t = (std::time_t)scenes[i].timestamp;
									std::tm*          tm = std::localtime(&t);
									std::stringstream ss;
									ss << std::put_time(tm, "%Y-%m-%d %H:%M:%S");
									label = ss.str();
								}
								bool isSelected = (selectedScene == i);
								if (ImGui::Selectable(label.c_str(), isSelected)) {
									selectedScene = i;
								}
							}
							ImGui::EndCombo();
						}

						ImGui::Checkbox("Move Camera", &m_moveCamera);
						ImGui::Checkbox("Apply Effects", &m_applyEffects);
						if (ImGui::Button("Load") && selectedScene >= 0) {
							const auto& scene = scenes[selectedScene];
							m_visualizer.GetLightManager().GetLights() = scene.lights;
							m_visualizer.GetLightManager().SetAmbientLight(scene.ambient_light);
							if (m_moveCamera && scene.camera) {
								m_visualizer.SetCamera(*scene.camera);
							}

							if (m_applyEffects) {
								m_visualizer.SetEffectEnabled(VisualEffect::RIPPLE, scene.object_effects.ripple);
								m_visualizer.SetEffectEnabled(
									VisualEffect::COLOR_SHIFT,
									scene.object_effects.color_shift
								);
								m_visualizer.SetEffectEnabled(
									VisualEffect::BLACK_AND_WHITE,
									scene.object_effects.black_and_white
								);
								m_visualizer.SetEffectEnabled(VisualEffect::NEGATIVE, scene.object_effects.negative);
								m_visualizer.SetEffectEnabled(VisualEffect::SHIMMERY, scene.object_effects.shimmery);
								m_visualizer.SetEffectEnabled(VisualEffect::GLITCHED, scene.object_effects.glitched);
								m_visualizer.SetEffectEnabled(VisualEffect::WIREFRAME, scene.object_effects.wireframe);

								auto& ppm = m_visualizer.GetPostProcessingManager();
								for (auto& effect : ppm.GetPreToneMappingEffects()) {
									if (auto bloom = std::dynamic_pointer_cast<PostProcessing::BloomEffect>(effect)) {
										bloom->SetEnabled(scene.post_processing.bloom_enabled);
										bloom->SetIntensity(scene.post_processing.bloom_intensity);
										bloom->SetThreshold(scene.post_processing.bloom_threshold);
									} else if (auto atmos =
												   std::dynamic_pointer_cast<PostProcessing::AtmosphereEffect>(effect)) {
										atmos->SetEnabled(scene.post_processing.atmosphere_enabled);
										atmos->SetHazeDensity(scene.post_processing.haze_density);
										atmos->SetHazeHeight(scene.post_processing.haze_height);
										atmos->SetHazeColor(scene.post_processing.haze_color);
										atmos->SetCloudDensity(scene.post_processing.cloud_density);
										atmos->SetCloudAltitude(scene.post_processing.cloud_altitude);
										atmos->SetCloudThickness(scene.post_processing.cloud_thickness);
										atmos->SetCloudColor(scene.post_processing.cloud_color);
									} else if (auto fg = std::dynamic_pointer_cast<PostProcessing::FilmGrainEffect>(
												   effect
											   )) {
										fg->SetEnabled(scene.post_processing.film_grain_enabled);
										fg->SetIntensity(scene.post_processing.film_grain_intensity);
									} else if (effect->GetName() == "Negative") {
										effect->SetEnabled(scene.post_processing.negative_enabled);
									} else if (effect->GetName() == "Glitch") {
										effect->SetEnabled(scene.post_processing.glitch_enabled);
									} else if (effect->GetName() == "OpticalFlow") {
										effect->SetEnabled(scene.post_processing.optical_flow_enabled);
									} else if (effect->GetName() == "Strobe") {
										effect->SetEnabled(scene.post_processing.strobe_enabled);
									} else if (effect->GetName() == "WhispTrail") {
										effect->SetEnabled(scene.post_processing.whisp_trail_enabled);
									} else if (effect->GetName() == "TimeStutter") {
										effect->SetEnabled(scene.post_processing.time_stutter_enabled);
									}
								}
								if (auto tm = ppm.GetToneMappingEffect()) {
									tm->SetEnabled(scene.post_processing.tone_mapping_enabled);
								}
							}
						}
						ImGui::SameLine();
						if (ImGui::Button("Remove") && selectedScene >= 0) {
							m_sceneManager.RemoveScene(selectedScene);
							m_sceneManager.SaveDictionary(currentDict);
							selectedScene = -1;
						}
					}
				}

				// 4. Picking Tool (from EffectsWidget)
				if (ImGui::CollapsingHeader("Tools", ImGuiTreeNodeFlags_DefaultOpen)) {
					if (ImGui::Button("Get World Coordinates")) {
						m_is_picking_enabled = true;
					}

					if (m_is_picking_enabled) {
						ImGui::Text("Click anywhere on the terrain to get the world coordinates.");
						if (ImGui::IsMouseClicked(0) && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)) {
							ImGuiIO& io = ImGui::GetIO();
							m_last_picked_pos = m_visualizer.ScreenToWorld(io.MousePos.x, io.MousePos.y);
							m_is_picking_enabled = false;
						}
					}

					if (m_last_picked_pos) {
						ImGui::Text(
							"Last Picked Position: X: %.2f, Y: %.2f, Z: %.2f",
							m_last_picked_pos->x,
							m_last_picked_pos->y,
							m_last_picked_pos->z
						);
					}
				}

				// 5. Generic Config (from ConfigWidget)
				RenderSection("global", true);
				auto&              config_manager = ConfigManager::GetInstance();
				const std::string& app_section = config_manager.GetAppSectionName();
				if (!app_section.empty()) {
					RenderSection(app_section, false);
				}
			}
			ImGui::End();
		}
	} // namespace UI
} // namespace Boidsish
