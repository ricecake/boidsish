#include "ui/EffectsWidget.h"

#include "ConfigManager.h"
#include "graphics.h"
#include "imgui.h"

namespace Boidsish {
	namespace UI {

		EffectsWidget::EffectsWidget(Visualizer* visualizer): m_visualizer(visualizer) {}

		void EffectsWidget::Draw() {
			if (!m_show) {
				return;
			}

			ImGui::Begin("Artistic Effects", &m_show);

			if (ImGui::Button("Get World Coordinates")) {
				m_is_picking_enabled = true;
			}

			if (m_is_picking_enabled) {
				ImGui::Text("Click anywhere on the terrain to get the world coordinates.");
				if (ImGui::IsMouseClicked(0) && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)) {
					ImGuiIO& io = ImGui::GetIO();
					m_last_picked_pos = m_visualizer->ScreenToWorld(io.MousePos.x, io.MousePos.y);
					m_is_picking_enabled = false;
				}
			}

			if (m_last_picked_pos) {
				ImGui::Text(
					"Last Picked Position: X: %.2f, Y: %.2f, Z: %.2f",
					m_last_picked_pos->x,
					m_last_picked_pos->y,
					m_last_picked_pos->z
				);
			}

			ImGui::Separator();

			auto& config = ConfigManager::GetInstance();

			bool ripple_enabled = config.GetAppSettingBool("artistic_effect_ripple", false);
			if (ImGui::Checkbox("Ripple", &ripple_enabled)) {
				config.SetBool("artistic_effect_ripple", ripple_enabled);
			}

			bool color_shift_enabled = config.GetAppSettingBool("artistic_effect_color_shift", false);
			if (ImGui::Checkbox("Color Shift", &color_shift_enabled)) {
				config.SetBool("artistic_effect_color_shift", color_shift_enabled);
			}

			bool bnw_enabled = config.GetAppSettingBool("artistic_effect_black_and_white", false);
			if (ImGui::Checkbox("Black and White", &bnw_enabled)) {
				config.SetBool("artistic_effect_black_and_white", bnw_enabled);
			}

			bool negative_enabled = config.GetAppSettingBool("artistic_effect_negative", false);
			if (ImGui::Checkbox("Negative", &negative_enabled)) {
				config.SetBool("artistic_effect_negative", negative_enabled);
			}

			bool shimmery_enabled = config.GetAppSettingBool("artistic_effect_shimmery", false);
			if (ImGui::Checkbox("Shimmery", &shimmery_enabled)) {
				config.SetBool("artistic_effect_shimmery", shimmery_enabled);
			}

			bool glitched_enabled = config.GetAppSettingBool("artistic_effect_glitched", false);
			if (ImGui::Checkbox("Glitched", &glitched_enabled)) {
				config.SetBool("artistic_effect_glitched", glitched_enabled);
			}

			bool wireframe_enabled = config.GetAppSettingBool("artistic_effect_wireframe", false);
			if (ImGui::Checkbox("Wireframe", &wireframe_enabled)) {
				config.SetBool("artistic_effect_wireframe", wireframe_enabled);
			}

			ImGui::Separator();
			ImGui::Text("Terrain Debug");

			float phong_alpha = m_visualizer->GetTerrainPhongAlpha();
			if (ImGui::SliderFloat("Phong Alpha", &phong_alpha, 0.0f, 1.0f)) {
				m_visualizer->SetTerrainPhongAlpha(phong_alpha);
			}

			bool terrain_debug_grid = m_visualizer->IsTerrainDebugGridEnabled();
			if (ImGui::Checkbox("Terrain Debug Grid (200x200)", &terrain_debug_grid)) {
				m_visualizer->SetTerrainDebugGridEnabled(terrain_debug_grid);
			}

			ImGui::End();
		}

	} // namespace UI
} // namespace Boidsish
