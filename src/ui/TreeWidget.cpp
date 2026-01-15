#include "ui/TreeWidget.h"
#include "imgui.h"

namespace Boidsish {
namespace UI {

TreeWidget::TreeWidget(TreeManager& treeManager) : m_treeManager(treeManager) {}

TreeWidget::~TreeWidget() {}

void TreeWidget::Draw() {
    ImGui::Begin("Tree Controls");
    if (ImGui::Button("Regenerate")) {
        m_treeManager.regenerate();
    }

    ImGui::SliderInt("Attraction Points", &m_treeManager.m_numAttractionPoints, 10, 2000);
    ImGui::SliderFloat("Attraction Radius", &m_treeManager.m_attractionRadius, 1.0f, 50.0f);
    ImGui::SliderFloat("Kill Radius", &m_treeManager.m_killRadius, 0.1f, 10.0f);
    ImGui::SliderFloat("Branch Length", &m_treeManager.m_branchLength, 0.1f, 5.0f);
    ImGui::SliderFloat3("Canopy Center", &m_treeManager.m_canopyCenter.x, -50.0f, 50.0f);
    ImGui::SliderFloat("Canopy Radius", &m_treeManager.m_canopyRadius, 1.0f, 50.0f);

    ImGui::End();
}

} // namespace UI
} // namespace Boidsish
