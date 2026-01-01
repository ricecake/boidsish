#include "ui/hud_widget.h"

#include <cstdio> // For snprintf

#include "hud_manager.h"
#include "imgui.h"
#include "logger.h"

namespace Boidsish {
	namespace UI {

		namespace {

			// Helper to calculate screen position based on alignment
			ImVec2 GetAlignmentPosition(HudAlignment alignment, const ImVec2& elementSize, const ImVec2& offset) {
				ImVec2 displaySize = ImGui::GetIO().DisplaySize;
				ImVec2 basePos;

				switch (alignment) {
				case HudAlignment::TOP_LEFT:
					basePos = {0, 0};
					break;
				case HudAlignment::TOP_CENTER:
					basePos = {(displaySize.x - elementSize.x) * 0.5f, 0};
					break;
				case HudAlignment::TOP_RIGHT:
					basePos = {displaySize.x - elementSize.x, 0};
					break;
				case HudAlignment::MIDDLE_LEFT:
					basePos = {0, (displaySize.y - elementSize.y) * 0.5f};
					break;
				case HudAlignment::MIDDLE_CENTER:
					basePos = {(displaySize.x - elementSize.x) * 0.5f, (displaySize.y - elementSize.y) * 0.5f};
					break;
				case HudAlignment::MIDDLE_RIGHT:
					basePos = {displaySize.x - elementSize.x, (displaySize.y - elementSize.y) * 0.5f};
					break;
				case HudAlignment::BOTTOM_LEFT:
					basePos = {0, displaySize.y - elementSize.y};
					break;
				case HudAlignment::BOTTOM_CENTER:
					basePos = {(displaySize.x - elementSize.x) * 0.5f, displaySize.y - elementSize.y};
					break;
				case HudAlignment::BOTTOM_RIGHT:
					basePos = {displaySize.x - elementSize.x, displaySize.y - elementSize.y};
					break;
				}

				return {basePos.x + offset.x, basePos.y + offset.y};
			}

		} // namespace

		HudWidget::HudWidget(HudManager& hudManager): m_hudManager(hudManager) {}

		void HudWidget::Draw() {
			logger::LOG("HudWidget::Draw called.");
			ImGui::SetNextWindowPos(ImVec2(0, 0));
			ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
			ImGui::Begin(
				"HUD",
				nullptr,
				ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground |
					ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBringToFrontOnFocus
			);
			logger::LOG("HUD window began.");

			// Draw Icons
			const auto& icons = m_hudManager.GetIcons();
			logger::LOG("Number of icons to draw: " + std::to_string(icons.size()));
			for (const auto& icon : icons) {
				ImVec2 size = {icon.size.x, icon.size.y};
				ImVec2 pos = GetAlignmentPosition(icon.alignment, size, {icon.position.x, icon.position.y});

				ImGui::SetCursorPos(pos);

				unsigned int textureId = m_hudManager.GetTextureId(icon.texture_path);
				logger::LOG("Icon " + std::to_string(icon.id) + " texture ID: " + std::to_string(textureId));
				if (textureId != 0) {
					ImVec4 tint_col = icon.highlighted ? ImVec4(1.0f, 1.0f, 0.0f, 1.0f)
													   : ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
					logger::LOG("Drawing icon " + std::to_string(icon.id));
					ImGui::Image(
						(void*)(intptr_t)textureId,
						size,
						ImVec2(0, 0),
						ImVec2(1, 1),
						tint_col,
						ImVec4(0, 0, 0, 0)
					);
					logger::LOG("Icon " + std::to_string(icon.id) + " drawn.");
				}
			}

			// Draw Numbers
			const auto& numbers = m_hudManager.GetNumbers();
			for (const auto& number : numbers) {
				char buffer[64];
				snprintf(
					buffer,
					sizeof(buffer),
					("%s: %." + std::to_string(number.precision) + "f").c_str(),
					number.label.c_str(),
					number.value
				);

				ImVec2 textSize = ImGui::CalcTextSize(buffer);
				ImVec2 pos = GetAlignmentPosition(number.alignment, textSize, {number.position.x, number.position.y});

				ImGui::SetCursorPos(pos);
				ImGui::Text("%s", buffer);
			}

			// Draw Gauges
			const auto& gauges = m_hudManager.GetGauges();
			for (const auto& gauge : gauges) {
				ImVec2 size = {gauge.size.x, gauge.size.y};
				ImVec2 pos = GetAlignmentPosition(gauge.alignment, size, {gauge.position.x, gauge.position.y});

				ImGui::SetCursorPos(pos);
				ImGui::ProgressBar(gauge.value, size, gauge.label.c_str());
			}

			ImGui::End();
		}

	} // namespace UI
} // namespace Boidsish
