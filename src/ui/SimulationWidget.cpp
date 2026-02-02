#include "ui/SimulationWidget.h"
#include "ConfigManager.h"
#include "graphics.h"
#include "imgui.h"
#include <vector>

namespace Boidsish {
	namespace UI {
		SimulationWidget::SimulationWidget(Visualizer& visualizer): m_visualizer(visualizer) {}

		void RenderConfigSection(const std::string& section_name, bool is_global) {
			auto& config_manager = ConfigManager::GetInstance();
			auto  registered_values = config_manager.GetRegisteredValues(section_name);
			if (registered_values.empty()) return;

			if (ImGui::TreeNode(section_name.c_str())) {
				// 1. Checkboxes
				for (const auto& pair : registered_values) {
					if (pair.second.type == ConfigValue::Type::BOOL) {
						if (pair.first.contains("enable_") || pair.first.contains("render_") || pair.first.contains("artistic_")) continue;
						bool value = is_global ? config_manager.GetGlobalSettingBool(pair.first, pair.second.bool_value)
											   : config_manager.GetAppSettingBool(pair.first, pair.second.bool_value);
						if (ImGui::Checkbox(pair.first.c_str(), &value)) config_manager.SetBool(pair.first, value);
					}
				}
				// 2. Sliders/Inputs
				for (const auto& pair : registered_values) {
					if (pair.second.type == ConfigValue::Type::INT) {
						if (pair.first.contains("window_")) continue;
						int value = is_global ? config_manager.GetGlobalSettingInt(pair.first, pair.second.int_value)
											  : config_manager.GetAppSettingInt(pair.first, pair.second.int_value);
						if (ImGui::InputInt(pair.first.c_str(), &value)) config_manager.SetInt(pair.first, value);
					} else if (pair.second.type == ConfigValue::Type::FLOAT) {
						if (pair.first.contains("camera_")) continue;
						float value = is_global ? config_manager.GetGlobalSettingFloat(pair.first, pair.second.float_value)
												: config_manager.GetAppSettingFloat(pair.first, pair.second.float_value);
						if (ImGui::InputFloat(pair.first.c_str(), &value)) config_manager.SetFloat(pair.first, value);
					}
				}
				// 3. Strings
				for (const auto& pair : registered_values) {
					if (pair.second.type == ConfigValue::Type::STRING) {
						char buffer[256];
						std::string current = is_global ? config_manager.GetGlobalSettingString(pair.first, pair.second.string_value)
														: config_manager.GetAppSettingString(pair.first, pair.second.string_value);
						strncpy(buffer, current.c_str(), sizeof(buffer) - 1);
						buffer[sizeof(buffer) - 1] = '\0';
						if (ImGui::InputText(pair.first.c_str(), buffer, sizeof(buffer))) config_manager.SetString(pair.first, std::string(buffer));
					}
				}
				ImGui::TreePop();
			}
		}

		void SimulationWidget::Draw() {
			if (!m_show) return;
			ImGui::Begin("Simulation Parameters", &m_show);
			RenderConfigSection("global", true);
			auto& config_manager = ConfigManager::GetInstance();
			const std::string& app_section = config_manager.GetAppSectionName();
			if (!app_section.empty()) RenderConfigSection(app_section, false);
			ImGui::Separator();
			bool paused = m_visualizer.GetPause();
			if (ImGui::Checkbox("Paused", &paused)) m_visualizer.SetPause(paused);
			float time_scale = m_visualizer.GetTimeScale();
			if (ImGui::SliderFloat("Time Scale", &time_scale, 0.01f, 5.0f)) m_visualizer.SetTimeScale(time_scale);
			ImGui::End();
		}
	}
}
