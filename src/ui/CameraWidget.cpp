#include "ui/CameraWidget.h"
#include "graphics.h"
#include "imgui.h"
namespace Boidsish {
	namespace UI {
		CameraWidget::CameraWidget(Visualizer& visualizer): m_visualizer(visualizer) {}
		void CameraWidget::Draw() {
			if (!m_show) return;
			ImGui::Begin("Camera Settings", &m_show);
			Camera& cam = m_visualizer.GetCamera();
			if (ImGui::TreeNode("Follow Settings")) {
				bool changed = false;
				if (ImGui::SliderFloat("Trail Distance", &cam.follow_distance, 0.0f, 50.0f)) changed = true;
				if (ImGui::SliderFloat("Elevation", &cam.follow_elevation, -20.0f, 20.0f)) changed = true;
				if (ImGui::SliderFloat("Look Ahead", &cam.follow_look_ahead, 0.0f, 50.0f)) changed = true;
				if (ImGui::SliderFloat("Responsiveness", &cam.follow_responsiveness, 0.1f, 20.0f)) changed = true;
				if (changed) m_visualizer.SetCamera(cam);
				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Path Following")) {
				bool changed = false;
				if (ImGui::SliderFloat("Path Smoothing", &cam.path_smoothing, 0.1f, 20.0f)) changed = true;
				if (ImGui::SliderFloat("Bank Factor", &cam.path_bank_factor, 0.0f, 5.0f)) changed = true;
				if (ImGui::SliderFloat("Bank Speed", &cam.path_bank_speed, 0.1f, 20.0f)) changed = true;
				if (changed) m_visualizer.SetCamera(cam);
				ImGui::TreePop();
			}
			ImGui::Separator();
			float camera_speed = cam.speed;
			if (ImGui::SliderFloat("Camera Speed", &camera_speed, 0.5f, 100.0f)) {
				cam.speed = camera_speed;
				m_visualizer.SetCamera(cam);
			}
			ImGui::Separator();
			if (ImGui::Button("Next Chase Target")) m_visualizer.CycleChaseTarget();
			if (ImGui::Button("Get World Coordinates")) m_is_picking_enabled = true;
			if (m_is_picking_enabled) {
				ImGui::Text("Click on terrain...");
				if (ImGui::IsMouseClicked(0) && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)) {
					ImGuiIO& io = ImGui::GetIO();
					m_last_picked_pos = m_visualizer.ScreenToWorld(io.MousePos.x, io.MousePos.y);
					m_is_picking_enabled = false;
				}
			}
			if (m_last_picked_pos) ImGui::Text("Last Picked: %.2f, %.2f, %.2f", m_last_picked_pos->x, m_last_picked_pos->y, m_last_picked_pos->z);
			const char* modes[] = {"Free", "Auto", "Tracking", "Stationary", "Chase", "Path Follow"};
			int current_mode = (int)m_visualizer.GetCameraMode();
			if (ImGui::Combo("Camera Mode", &current_mode, modes, IM_ARRAYSIZE(modes))) m_visualizer.SetCameraMode((CameraMode)current_mode);
			ImGui::End();
		}
	}
}
