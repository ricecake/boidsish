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

    int numAttractionPoints = m_treeManager.GetNumAttractionPoints();
    if (ImGui::SliderInt("Attraction Points", &numAttractionPoints, 10, 2000)) {
        m_treeManager.SetNumAttractionPoints(numAttractionPoints);
    }

    float attractionRadius = m_treeManager.GetAttractionRadius();
    if (ImGui::SliderFloat("Attraction Radius", &attractionRadius, 1.0f, 50.0f)) {
        m_treeManager.SetAttractionRadius(attractionRadius);
    }

    float killRadius = m_treeManager.GetKillRadius();
    if (ImGui::SliderFloat("Kill Radius", &killRadius, 0.1f, 10.0f)) {
        m_treeManager.SetKillRadius(killRadius);
    }

    float branchLength = m_treeManager.GetBranchLength();
    if (ImGui::SliderFloat("Branch Length", &branchLength, 0.1f, 5.0f)) {
        m_treeManager.SetBranchLength(branchLength);
    }

    glm::vec3 canopyCenter = m_treeManager.GetCanopyCenter();
    if (ImGui::SliderFloat3("Canopy Center", &canopyCenter.x, -50.0f, 50.0f)) {
        m_treeManager.SetCanopyCenter(canopyCenter);
    }

    float canopyRadius = m_treeManager.GetCanopyRadius();
    if (ImGui::SliderFloat("Canopy Radius", &canopyRadius, 1.0f, 50.0f)) {
        m_treeManager.SetCanopyRadius(canopyRadius);
    }

    ImGui::End();
}

} // namespace UI
} // namespace Boidsish
