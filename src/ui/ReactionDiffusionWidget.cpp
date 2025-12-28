#include "ui/ReactionDiffusionWidget.h"
#include "imgui.h"

namespace Boidsish {
namespace UI {

    ReactionDiffusionWidget::ReactionDiffusionWidget(ReactionDiffusionManager& manager)
        : m_manager(manager), m_name("Reaction Diffusion") {
    }

    void ReactionDiffusionWidget::Draw() {
        if (ImGui::Begin(m_name.c_str())) {
            ImGui::Text("Simulation Parameters");
            ImGui::SliderFloat("Feed Rate", &m_manager.GetFeedRate(), 0.0f, 0.1f);
            ImGui::SliderFloat("Kill Rate", &m_manager.GetKillRate(), 0.0f, 0.1f);
            ImGui::SliderFloat("Diffuse Rate A", &m_manager.GetDiffuseRateA(), 0.0f, 2.0f);
            ImGui::SliderFloat("Diffuse Rate B", &m_manager.GetDiffuseRateB(), 0.0f, 2.0f);
            ImGui::SliderFloat("Timestep", &m_manager.GetTimestep(), 0.1f, 2.0f);
            ImGui::SliderInt("Iterations", &m_manager.GetIterations(), 1, 50);

            ImGui::Separator();

            ImGui::Checkbox("Pause", &m_manager.IsPaused());
            if (ImGui::Button("Reset")) {
                m_manager.Reset();
            }
        }
        ImGui::End();
    }

}
}
