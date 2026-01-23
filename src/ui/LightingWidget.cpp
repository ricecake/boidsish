#include "ui/LightingWidget.h"

#include "graphics.h"
#include "light_manager.h"
#include "imgui.h"
#include <string>

namespace Boidsish {
    namespace UI {

        LightingWidget::LightingWidget(Visualizer& visualizer)
            : m_visualizer(visualizer) {}

        void LightingWidget::Draw() {
            if (!m_show) {
                return;
            }

            ImGui::Begin("Lighting", &m_show);

            auto& lightManager = m_visualizer.GetLightManager();
            auto& lights = lightManager.GetLights();

            int light_to_remove = -1;

            for (int i = 0; i < lights.size(); ++i) {
                std::string light_name = "Light " + std::to_string(i);
                if (ImGui::CollapsingHeader(light_name.c_str())) {
                    std::string pos_label = "Position##" + std::to_string(i);
                    ImGui::DragFloat3(pos_label.c_str(), &lights[i].position.x, 0.1f);

                    std::string color_label = "Color##" + std::to_string(i);
                    ImGui::ColorEdit3(color_label.c_str(), &lights[i].color.x);

                    std::string intensity_label = "Intensity##" + std::to_string(i);
                    ImGui::DragFloat(intensity_label.c_str(), &lights[i].intensity, 0.1f, 0.0f, 1000.0f);

                    if (lights.size() > 1) {
                        std::string remove_label = "Remove##" + std::to_string(i);
                        if (ImGui::Button(remove_label.c_str())) {
                           light_to_remove = i;
                        }
                    }
                }
            }

            if (light_to_remove != -1) {
                lights.erase(lights.begin() + light_to_remove);
            }

            if (ImGui::Button("Add Light")) {
                lightManager.AddLight(Light::Create({0, 10, 0}, 5.0f, {1, 1, 1}, false));
            }


            ImGui::End();
        }

    } // namespace UI
} // namespace Boidsish
