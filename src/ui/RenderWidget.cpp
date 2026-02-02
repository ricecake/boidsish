#include "ui/RenderWidget.h"
#include "ConfigManager.h"
#include "graphics.h"
#include "imgui.h"
namespace Boidsish {
	namespace UI {
		RenderWidget::RenderWidget(Visualizer& visualizer): m_visualizer(visualizer) {}
		void RenderWidget::Draw() {
			if (!m_show) return;
			ImGui::Begin("Render Settings", &m_show);
			auto& config = ConfigManager::GetInstance();
			if (ImGui::TreeNode("Visibility")) {
				bool terrain = config.GetAppSettingBool("render_terrain", true);
				if (ImGui::Checkbox("Terrain", &terrain)) config.SetBool("render_terrain", terrain);
				bool floor = config.GetAppSettingBool("render_floor", true);
				if (ImGui::Checkbox("Floor", &floor)) config.SetBool("render_floor", floor);
				bool skybox = config.GetAppSettingBool("render_skybox", true);
				if (ImGui::Checkbox("Skybox", &skybox)) config.SetBool("render_skybox", skybox);
				ImGui::TreePop();
			}
			ImGui::Separator();
			bool fullscreen = config.GetAppSettingBool("fullscreen", false);
			if (ImGui::Checkbox("Fullscreen", &fullscreen)) config.SetBool("fullscreen", fullscreen);
			bool hdr = config.GetAppSettingBool("enable_hdr", false);
			if (ImGui::Checkbox("HDR", &hdr)) config.SetBool("enable_hdr", hdr);
			bool gl_debug = config.GetAppSettingBool("enable_gl_debug", false);
			if (ImGui::Checkbox("OpenGL Debug", &gl_debug)) config.SetBool("enable_gl_debug", gl_debug);
			bool reflection = config.GetAppSettingBool("enable_floor_reflection", true);
			if (ImGui::Checkbox("Floor Reflection", &reflection)) config.SetBool("enable_floor_reflection", reflection);
			ImGui::Separator();
			int width = config.GetAppSettingInt("window_width", 1200);
			if (ImGui::InputInt("Width", &width)) config.SetInt("window_width", width);
			int height = config.GetAppSettingInt("window_height", 800);
			if (ImGui::InputInt("Height", &height)) config.SetInt("window_height", height);
			ImGui::End();
		}
	}
}
