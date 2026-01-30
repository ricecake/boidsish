#include "ui/ConfigWidget.h"

#include "ConfigManager.h"
#include "graphics.h"
#include "imgui.h"

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

					if (key.contains("artistic_effect_")) {
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
