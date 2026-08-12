#include "ui/CompetitiveCAWidget.h"
#include "graphics.h"
#include "imgui.h"
#include <iostream>
#include <algorithm>

namespace Boidsish {
	namespace UI {

		CompetitiveCAWidget::CompetitiveCAWidget(Visualizer& visualizer) :
			m_visualizer(visualizer) {
		}

		void CompetitiveCAWidget::Draw() {
			if (!m_show)
				return;

			if (!m_initialized) {
				m_ca.Initialize(256, 256, m_mode);
				m_initialized = true;
			}

			// Advance the CA step when running
			if (!m_paused) {
				m_elapsed_time += m_dt;
				m_ca.Step(
					m_dt,
					m_mode,
					m_growth,
					m_competition,
					m_diffusion,
					m_sharpness,
					m_fuzziness,
					m_radius,
					m_elapsed_time
				);
			}

			ImGui::SetNextWindowSize(ImVec2(380, 720), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowCollapsed(true, ImGuiCond_Once);
			if (ImGui::Begin("Competitive Cellular Automata", &m_show)) {
				ImGui::Text("Biological / Competitive Texture Synthesizer");
				ImGui::Separator();

				// Real-time Texture view
				ImGui::Spacing();
				ImVec2 contentRegion = ImGui::GetContentRegionAvail();
				float displaySize = std::min(contentRegion.x, 340.0f);
				ImGui::SetCursorPosX((contentRegion.x - displaySize) * 0.5f);
				ImGui::Image(
					(void*)(intptr_t)m_ca.GetDisplayTexture(),
					ImVec2(displaySize, displaySize),
					ImVec2(0, 1),
					ImVec2(1, 0)
				);
				ImGui::Spacing();

				// Playback controls
				if (m_paused) {
					if (ImGui::Button("Play", ImVec2(80, 0))) {
						m_paused = false;
					}
					ImGui::SameLine();
					if (ImGui::Button("Step", ImVec2(80, 0))) {
						m_elapsed_time += m_dt;
						m_ca.Step(
							m_dt,
							m_mode,
							m_growth,
							m_competition,
							m_diffusion,
							m_sharpness,
							m_fuzziness,
							m_radius,
							m_elapsed_time
						);
					}
				} else {
					if (ImGui::Button("Pause", ImVec2(80, 0))) {
						m_paused = true;
					}
				}

				ImGui::Separator();
				ImGui::TextColored(ImVec4(0, 1, 1, 1), "System Parameters:");

				// System Mode
				const char* modes[] = {
					"Continuous Growth",
					"Continuous Cyclic (RPS)",
					"Soft Voting / Threshold",
					"Energy-Conserving (2-Pass)"
				};
				int old_mode = m_mode;
				if (ImGui::Combo("CA Mode", &m_mode, modes, 4)) {
					if (old_mode != m_mode) {
						// Re-initialize texture configurations on mode transition
						m_ca.Initialize(256, 256, m_mode);
					}
				}

				ImGui::SliderInt("Species Count", &m_species_count, 1, 4);

				// Only show neighborhood radius for continuous-state modes
				if (m_mode < 3) {
					ImGui::SliderInt("Lobe Radius", &m_radius, 1, 5);
				}

				ImGui::SliderFloat("Growth Rate", &m_growth, 0.0f, 5.0f, "%.2f");

				if (m_mode == 3) {
					ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Note: Competition acts as seed threshold.");
				}
				ImGui::SliderFloat(m_mode == 3 ? "Threshold" : "Competition / Limit", &m_competition, 0.0f, 5.0f, "%.2f");

				if (m_mode < 3) {
					ImGui::SliderFloat("Diffusion / Blend", &m_diffusion, 0.0f, 2.0f, "%.2f");
					ImGui::SliderFloat("Sharpness", &m_sharpness, 0.0f, 5.0f, "%.2f");
					ImGui::SliderFloat("Edge Fuzziness", &m_fuzziness, 0.0f, 1.0f, "%.2f");
				}

				ImGui::SliderFloat("Speed (dt)", &m_dt, 0.01f, 2.0f, "%.2f");

				ImGui::Separator();
				ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), "Seeding & Reset:");

				const char* patterns[] = { "Random Noise", "Central Quadrants", "Grid of Seeds", "Clear" };
				ImGui::Combo("Seed Pattern", &m_seed_pattern, patterns, 4);

				if (ImGui::Button("Re-seed Grid", ImVec2(120, 0))) {
					m_ca.Seed(m_seed_pattern, m_species_count, m_mode);
				}

				ImGui::Spacing();
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Channels represent distinct color cells:");
				ImGui::TextColored(ImVec4(1, 0, 0, 1), "Red channel = Species A");
				ImGui::TextColored(ImVec4(0, 1, 0, 1), "Green channel = Species B");
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 1, 1), "Blue channel = Species C");
				ImGui::TextColored(ImVec4(1, 1, 1, 1), "Alpha channel = Species D (Yellow)");
			}
			ImGui::End();
		}

	} // namespace UI
} // namespace Boidsish
