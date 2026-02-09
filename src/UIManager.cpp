#include "UIManager.h"

#include <algorithm>

#include "IWidget.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"

namespace Boidsish {
	namespace UI {
		UIManager::UIManager(GLFWwindow* window) {
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();
			ImGuiIO& io = ImGui::GetIO();
			(void)io;
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
			// io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

			ImGui::LoadIniSettingsFromDisk("imgui.ini");

			ImGui::StyleColorsDark();

			ImGui_ImplGlfw_InitForOpenGL(window, true);
			ImGui_ImplOpenGL3_Init("#version 130");
		}

		UIManager::~UIManager() {
			ImGui::SaveIniSettingsToDisk("imgui.ini");
			ImGui_ImplOpenGL3_Shutdown();
			ImGui_ImplGlfw_Shutdown();
			ImGui::DestroyContext();
		}

		void UIManager::AddWidget(std::shared_ptr<IWidget> widget) {
			m_widgets.push_back(widget);
		}

		void UIManager::Render() {
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			for (const auto& widget : m_widgets) {
				if (widget->IsHud() || m_show_menus) {
					widget->Draw();
				}
			}

			PositionMinimizedWindows();

			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		}

		void UIManager::PositionMinimizedWindows() {
			ImGuiContext& g = *GImGui;
			ImGuiIO&      io = ImGui::GetIO();

			float corner_x = 10.0f;
			float corner_y = io.DisplaySize.y - 10.0f;
			float current_y = corner_y;

			std::vector<ImGuiWindow*> collapsed_windows;

			for (int i = 0; i < g.Windows.Size; i++) {
				ImGuiWindow* window = g.Windows[i];

				// Skip hidden windows, child windows, or windows not active this frame
				if (window->Hidden || (window->Flags & ImGuiWindowFlags_ChildWindow))
					continue;

				if (window->LastFrameActive < g.FrameCount)
					continue;

				// Skip internal windows (like tooltips or popups that aren't widgets)
				// We mostly care about top-level windows that have titles.
				if (window->Flags & ImGuiWindowFlags_Tooltip || window->Flags & ImGuiWindowFlags_Popup)
					continue;

				WindowState& state = m_window_states[window->ID];

				if (window->Collapsed) {
					if (!state.was_collapsed) {
						// Just collapsed, save current position
						state.last_expanded_pos = window->Pos;
						state.was_collapsed = true;
					}
					collapsed_windows.push_back(window);
				} else {
					if (state.was_collapsed) {
						// Just uncollapsed, restore previous position
						window->Pos = state.last_expanded_pos;
						state.was_collapsed = false;
					} else {
						// Update expanded position as user moves it
						state.last_expanded_pos = window->Pos;
					}
				}
			}

			// Sort collapsed windows by ID for stable positioning
			std::sort(collapsed_windows.begin(), collapsed_windows.end(), [](ImGuiWindow* a, ImGuiWindow* b) {
				return a->ID < b->ID;
			});

			for (ImGuiWindow* window : collapsed_windows) {
				// Position in corner
				float window_height = window->TitleBarHeight;
				current_y -= window_height;
				window->Pos = ImVec2(corner_x, current_y);
				current_y -= 5.0f; // Padding
			}
		}
	} // namespace UI
} // namespace Boidsish
