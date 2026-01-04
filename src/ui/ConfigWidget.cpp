#include "ui/ConfigWidget.h"
#include "ConfigManager.h"
#include "imgui.h"

namespace Boidsish {
    namespace UI {

        ConfigWidget::ConfigWidget() {}

        void RenderSection(const std::string& section_name) {
            if (ImGui::CollapsingHeader(section_name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& config_manager = ConfigManager::GetInstance();
                auto registered_values = config_manager.GetRegisteredValues(section_name);

                for (const auto& pair : registered_values) {
                    const std::string& key = pair.first;
                    const ConfigValue& val_info = pair.second;

                    switch (val_info.type) {
                        case ConfigValue::Type::BOOL: {
                            bool value = config_manager.GetAppSettingBool(key, val_info.bool_value);
                            if (ImGui::Checkbox(key.c_str(), &value)) {
                                config_manager.SetBool(key, value);
                            }
                            break;
                        }
                        case ConfigValue::Type::INT: {
                            int value = config_manager.GetAppSettingInt(key, val_info.int_value);
                            if (ImGui::InputInt(key.c_str(), &value)) {
                                config_manager.SetInt(key, value);
                            }
                            break;
                        }
                        case ConfigValue::Type::FLOAT: {
                            float value = config_manager.GetAppSettingFloat(key, val_info.float_value);
                            if (ImGui::InputFloat(key.c_str(), &value)) {
                                config_manager.SetFloat(key, value);
                            }
                            break;
                        }
                        case ConfigValue::Type::STRING: {
                            char buffer[256];
                            std::string current_val = config_manager.GetAppSettingString(key, val_info.string_value);
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
            if (!m_show) return;

            if (ImGui::Begin("Configuration", &m_show)) {
                RenderSection("global");

                auto& config_manager = ConfigManager::GetInstance();
                const std::string& app_section = config_manager.GetAppSectionName();
                if (!app_section.empty()) {
                    RenderSection(app_section);
                }
            }
            ImGui::End();
        }
    }
}
