#include "ui/ConfigWidget.h"

#include "ConfigManager.h"
#include "decor_manager.h"
#include "graphics.h"
#include "imgui.h"
#include "terrain_generator_interface.h"

namespace Boidsish {
	namespace UI {

		ConfigWidget::ConfigWidget(Visualizer& visualizer): m_visualizer(visualizer) {}

		void RenderSection(const std::string& section_name, bool is_global) {
			if (ImGui::CollapsingHeader(section_name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
				auto& config_manager = ConfigManager::GetInstance();
				auto  registered_values = config_manager.GetRegisteredValues(section_name);

				for (const auto& pair : registered_values) {
					const std::string& key = pair.first;
					const ConfigValue& val_info = pair.second;

					if (key.contains("artistic_effect_") || key == "render_scale" || key == "enable_shadows" ||
					    key == "enable_floor_reflection") {
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

		void ConfigWidget::Draw() {
			if (!m_show)
				return;

			if (ImGui::Begin("Configuration", &m_show)) {
				if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
					// Camera speed slider
					float camera_speed = m_visualizer.GetCamera().speed;
					if (ImGui::SliderFloat("Speed", &camera_speed, 0.5f, 100.0f)) {
						Camera cam = m_visualizer.GetCamera();
						cam.speed = camera_speed;
						m_visualizer.SetCamera(cam);
					}

					// Camera mode dropdown
					const char* modes[] = {"Free", "Auto", "Tracking", "Stationary", "Chase", "Path Follow"};
					int         current_mode = static_cast<int>(m_visualizer.GetCameraMode());
					if (ImGui::Combo("Mode", &current_mode, modes, IM_ARRAYSIZE(modes))) {
						m_visualizer.SetCameraMode(static_cast<CameraMode>(current_mode));
					}

					if (ImGui::Button("Next Chase Target")) {
						m_visualizer.CycleChaseTarget();
					}
				}

				if (ImGui::CollapsingHeader("Follow Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
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

				if (ImGui::CollapsingHeader("Time", ImGuiTreeNodeFlags_DefaultOpen)) {
					// Camera speed slider
					float time_scale = m_visualizer.GetTimeScale();
					if (ImGui::SliderFloat("Time Scale", &time_scale, 0.01f, 5.0f)) {
						m_visualizer.SetTimeScale(time_scale);
					}

					bool paused = m_visualizer.GetPause();
					if (ImGui::Checkbox("Paused", &paused)) {
						m_visualizer.SetPause(paused);
					}
				}

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

				if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
					float render_scale = m_visualizer.GetRenderScale();
					if (ImGui::SliderFloat("Render Scale", &render_scale, 0.1f, 1.0f)) {
						m_visualizer.SetRenderScale(render_scale);
					}

					auto& config_manager = ConfigManager::GetInstance();
					bool  enable_shadows = config_manager.GetAppSettingBool("enable_shadows", true);
					if (ImGui::Checkbox("Enable Shadows", &enable_shadows)) {
						config_manager.SetBool("enable_shadows", enable_shadows);
					}

					bool enable_reflections = config_manager.GetAppSettingBool("enable_floor_reflection", true);
					if (ImGui::Checkbox("Enable Floor Reflections", &enable_reflections)) {
						config_manager.SetBool("enable_floor_reflection", enable_reflections);
					}
				}

				if (ImGui::CollapsingHeader("Atmosphere", ImGuiTreeNodeFlags_DefaultOpen)) {
					auto& config = ConfigManager::GetInstance();

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

					float sun_intensity_factor = config.GetAppSettingFloat("sun_intensity_factor", 35.0f);
					if (ImGui::SliderFloat("Sun Intensity Factor", &sun_intensity_factor, 1.0f, 100.0f)) {
						config.SetFloat("sun_intensity_factor", sun_intensity_factor);
					}

					float rayleigh_scale_height = config.GetAppSettingFloat("rayleigh_scale_height", 100.0f);
					if (ImGui::SliderFloat("Rayleigh Scale Height", &rayleigh_scale_height, 10.0f, 500.0f)) {
						config.SetFloat("rayleigh_scale_height", rayleigh_scale_height);
					}

					float mie_scale_height = config.GetAppSettingFloat("mie_scale_height", 15.0f);
					if (ImGui::SliderFloat("Mie Scale Height", &mie_scale_height, 1.0f, 100.0f)) {
						config.SetFloat("mie_scale_height", mie_scale_height);
					}

					ImGui::Separator();
					ImGui::Text("Clouds");
					bool enable_clouds = config.GetAppSettingBool("enable_clouds", true);
					if (ImGui::Checkbox("Enable Clouds", &enable_clouds)) {
						config.SetBool("enable_clouds", enable_clouds);
					}

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

					ImGui::Separator();
					bool enable_fog = config.GetAppSettingBool("enable_fog", true);
					if (ImGui::Checkbox("Enable Fog/Haze", &enable_fog)) {
						config.SetBool("enable_fog", enable_fog);
					}
				}

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
