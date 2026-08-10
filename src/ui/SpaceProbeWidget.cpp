#include "ui/SpaceProbeWidget.h"

#include "imgui.h"
#include "graphics.h"
#include "SpaceProbeManager.h"

namespace Boidsish {
	namespace UI {

		SpaceProbeWidget::SpaceProbeWidget(Visualizer& visualizer) : m_visualizer(visualizer) {
			m_show = true; // Open by default, alongside other top-level menus
		}

		void SpaceProbeWidget::Draw() {
			if (!m_show) return;

			auto probe = m_visualizer.GetSpaceProbeManager();
			if (!probe) return;

			ImGui::SetNextWindowPos(ImVec2(1060, 20), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(350, 520), ImGuiCond_FirstUseEver);

			if (ImGui::Begin("Space Probe Analyzer", &m_show)) {
				ImGui::Text("Measure properties at a specific point in space.");
				ImGui::Separator();

				// --- Main Control Toggles ---
				ImGui::Checkbox("Enable Probe System", &probe->is_enabled);
				ImGui::Checkbox("Render Sphere Overlay", &probe->render_sphere);
				ImGui::Checkbox("Update Continuously", &probe->continuous_update);
				ImGui::Checkbox("SH Visualizer Mode", &probe->use_sh_visualizer);
				ImGui::Checkbox("Camera Follow Mode (F2)", &probe->camera_follow_mode);

				ImGui::Separator();
				ImGui::Text("--- Interactive Actions ---");

				if (probe->camera_follow_mode) {
					ImGui::BeginDisabled();
				}

				// --- Interactive Position & Control Buttons ---
				if (ImGui::Button("Move Probe to Camera")) {
					probe->position = m_visualizer.GetCamera().pos();
				}
				ImGui::SameLine();
				if (ImGui::Button("Refresh Probe Data")) {
					probe->refresh_requested = true;
				}

				ImGui::DragFloat3("Probe Coordinates", &probe->position[0], 0.1f);

				if (probe->camera_follow_mode) {
					ImGui::EndDisabled();
					ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Coordinates locked to Camera Follow Mode");
				}

				ImGui::SliderFloat("Sphere Radius", &probe->sphere_radius, 0.1f, 20.0f);

				ImGui::Separator();
				ImGui::Text("--- Sphere Material Settings ---");

				// --- Sphere Material Properties (roughness, metallic, etc) ---
				ImGui::ColorEdit3("Material Color", &probe->sphere_color[0]);
				ImGui::SliderFloat("Metallic", &probe->metallic, 0.0f, 1.0f);
				ImGui::SliderFloat("Roughness", &probe->roughness, 0.0f, 1.0f);

				ImGui::Separator();

				// --- Numeric Outputs & Measured Values (Concrete real-world units where possible) ---
				const auto& data = probe->GetData();

				ImGui::Text("--- Measured Values ---");
				ImGui::Text("Total Shadow Level: %.3f", data.shadow_level);
				ImGui::ProgressBar(data.shadow_level, ImVec2(-1, 0));

				ImGui::Text("Terrain Occlusion Shadow: %.3f", data.shadow_from_terrain);
				ImGui::Text("Cloud Density Shadow:     %.3f", data.cloud_shadow);

				ImGui::Text("Ambient Occlusion (AO):   %.3f", data.ambient_occlusion);
				ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.6f, 0.8f, 1.0f));
				ImGui::ProgressBar(data.ambient_occlusion, ImVec2(-1, 0));
				ImGui::PopStyleColor();

				ImGui::Separator();
				ImGui::Text("Arriving Irradiance / Illuminance:");

				ImGui::Text("Directional Light (Sun/Moon):");
				ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.6f, 1.0f), "  %.1f Lux (R: %.1f, G: %.1f, B: %.1f)",
					(data.light_directional.r + data.light_directional.g + data.light_directional.b) / 3.0f,
					data.light_directional.r, data.light_directional.g, data.light_directional.b);

				ImGui::Text("Clustered Local Lights (Other):");
				ImGui::TextColored(ImVec4(0.6f, 0.9f, 1.0f, 1.0f), "  %.1f Lux (R: %.1f, G: %.1f, B: %.1f)",
					(data.light_other.r + data.light_other.g + data.light_other.b) / 3.0f,
					data.light_other.r, data.light_other.g, data.light_other.b);

				ImGui::Text("Ambient Environment Light:");
				ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "  %.1f Lux (R: %.1f, G: %.1f, B: %.1f)",
					(data.light_ambient.r + data.light_ambient.g + data.light_ambient.b) / 3.0f,
					data.light_ambient.r, data.light_ambient.g, data.light_ambient.b);
			}
			ImGui::End();
		}

	} // namespace UI
} // namespace Boidsish
