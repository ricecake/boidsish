#include "ui/ScriptWidget.h"
#include "ScriptManager.h"
#include "imgui.h"
#include <iostream>

namespace Boidsish {
	namespace UI {

		ScriptWidget::ScriptWidget(ScriptManager& scriptManager): m_scriptManager(scriptManager) {
			m_inputBuffer[0] = '\0';
		}

		void ScriptWidget::Draw() {
			if (!m_show)
				return;

			ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("Script Console", &m_show)) {
				// History area
				const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
				ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar);

				for (const auto& entry : m_history) {
					ImGui::TextColored(ImVec4(0.5f, 0.5f, 1.0f, 1.0f), "> %s", entry.first.c_str());
					ImGui::TextUnformatted(entry.second.c_str());
				}

				if (m_scrollToBottom || ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
					ImGui::SetScrollHereY(1.0f);
				m_scrollToBottom = false;

				ImGui::EndChild();

				ImGui::Separator();

				// Input area
				bool reclaim_focus = false;
				ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue;
				if (ImGui::InputText("##Input", m_inputBuffer, IM_ARRAYSIZE(m_inputBuffer), input_text_flags)) {
					std::string cmd = m_inputBuffer;
					if (!cmd.empty()) {
						std::string result = m_scriptManager.Execute(cmd);
						m_history.push_back({cmd, result});
						m_scrollToBottom = true;
					}
					m_inputBuffer[0] = '\0';
					reclaim_focus = true;
				}

				// Auto-focus on window apparition
				ImGui::SetItemDefaultFocus();
				if (reclaim_focus)
					ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget
			}
			ImGui::End();
		}

	} // namespace UI
} // namespace Boidsish
