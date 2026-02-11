#include "ui/hud_widget.h"
#include "hud_manager.h"
#include "imgui.h"

namespace Boidsish {
	namespace UI {

		HudWidget::HudWidget(HudManager& hudManager): m_hudManager(hudManager) {}

		void HudWidget::Draw() {
			ImGui::SetNextWindowPos(ImVec2(0, 0));
			ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
			ImGui::Begin(
				"HUD",
				nullptr,
				ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground |
					ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBringToFrontOnFocus
			);

			for (const auto& element : m_hudManager.GetElements()) {
				if (element && element->IsVisible()) {
					element->Draw(m_hudManager);
				}
			}

			ImGui::End();
		}

	} // namespace UI
} // namespace Boidsish
