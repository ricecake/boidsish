#include "ui/LightingWidget.h"

#include "graphics.h"
#include "imgui.h"

namespace Boidsish {
	namespace UI {

		LightingWidget::LightingWidget(Visualizer& visualizer)
			: m_visualizer(visualizer) {}

		void LightingWidget::Draw() {
			if (!m_show)
				return;

			ImGui::SetNextWindowPos(ImVec2(20, 330), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("Lighting", &m_show)) {
				if (ImGui::CollapsingHeader("Ambient Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
					m_ambientSettings.Draw(m_visualizer);
				}
				if (ImGui::CollapsingHeader("Individual Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
					m_lights.Draw(m_visualizer);
				}
				if (ImGui::CollapsingHeader("Bloom Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
					m_bloomSettings.Draw(m_visualizer);
				}
			}
			ImGui::End();
		}

	} // namespace UI
} // namespace Boidsish
