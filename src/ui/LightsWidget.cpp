#include "ui/LightsWidget.h"
#include "imgui.h"

namespace Boidsish {
namespace UI {

LightsWidget::LightsWidget(LightManager& lightManager)
    : _lightManager(lightManager) {}

void LightsWidget::Draw() {
    if (!m_show) {
        return;
    }

    ImGui::Begin("Lights", &m_show);

    // Ambient Light
    glm::vec3 ambient = _lightManager.GetAmbientLight();
    if (ImGui::ColorEdit3("Ambient Light", &ambient[0])) {
        _lightManager.SetAmbientLight(ambient);
    }

    ImGui::Separator();

    // Lights
    auto& lights = _lightManager.GetLights();
    for (int i = 0; i < lights.size(); ++i) {
        ImGui::PushID(i);
        if (ImGui::TreeNode("Light", "Light %d", i)) {
            // Type
            const char* types[] = { "Point", "Directional", "Spot" };
            int current_type = lights[i].type;
            if (ImGui::Combo("Type", &current_type, types, IM_ARRAYSIZE(types))) {
                lights[i].type = (LightType)current_type;
            }

            // Position
            ImGui::DragFloat3("Position", &lights[i].position[0], 0.1f);

            // Direction
            if (lights[i].type != POINT_LIGHT) {
                ImGui::DragFloat3("Direction", &lights[i].direction[0], 0.1f);
            }

            // Color
            ImGui::ColorEdit3("Color", &lights[i].color[0]);

            // Intensity
            ImGui::DragFloat("Intensity", &lights[i].intensity, 0.1f);

            // Cutoffs
            if (lights[i].type == SPOT_LIGHT) {
                float inner_angle = glm::degrees(glm::acos(lights[i].inner_cutoff));
                float outer_angle = glm::degrees(glm::acos(lights[i].outer_cutoff));

                if (ImGui::DragFloat("Inner Angle", &inner_angle, 0.1f, 0.0f, 90.0f)) {
                    if (inner_angle > outer_angle) {
                        inner_angle = outer_angle;
                    }
                    lights[i].inner_cutoff = glm::cos(glm::radians(inner_angle));
                }
                if (ImGui::DragFloat("Outer Angle", &outer_angle, 0.1f, 0.0f, 90.0f)) {
                    if (inner_angle > outer_angle) {
                        outer_angle = inner_angle;
                    }
                    lights[i].outer_cutoff = glm::cos(glm::radians(outer_angle));
                }
            }

            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    ImGui::End();
}

} // namespace UI
} // namespace Boidsish
