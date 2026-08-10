#include "UIConfigManager.h"

#include <algorithm>

#include "IWidget.h"
#include "graphics.h"
#include "SceneManager.h"
#include "hud_manager.h"
#include "light_manager.h"
#include "mood_manager.h"
#include "hud.h"
#include "ui/EnvironmentWidget.h"
#include "ui/MoodWidget.h"
#include "ui/LightningWidget.h"
#include "ui/EffectWidget.h"
#include "ui/RenderWidget.h"
#include "ui/LightingWidget.h"
#include "ui/AudioWidget.h"
#include "ui/SystemWidget.h"
#include "ui/ProfilerWidget.h"
#include "ui/SpaceProbeWidget.h"
#include "ui/hud_widget.h"
#include "service_locator.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"
#include "profiler.h"

namespace Boidsish {
	namespace UI {
		UIConfigManager::UIConfigManager(ServiceLocator& /*loc*/, GLFWwindow* window) {
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

		UIConfigManager::~UIConfigManager() {
			ImGui::SaveIniSettingsToDisk("imgui.ini");
			ImGui_ImplOpenGL3_Shutdown();
			ImGui_ImplGlfw_Shutdown();
			ImGui::DestroyContext();
		}

		void UIConfigManager::AddWidget(std::shared_ptr<IWidget> widget) {
			m_widgets.push_back(widget);
		}

		void UIConfigManager::SetupDefaultWidgets(Visualizer& visualizer, SceneManager& scene_manager, HudManager& hud_manager) {
			m_visualizer = &visualizer;
			AddWidget(std::make_shared<HudWidget>(hud_manager));
			AddWidget(std::make_shared<EnvironmentWidget>(visualizer));
			AddWidget(std::make_shared<MoodWidget>(visualizer));
			AddWidget(std::make_shared<LightningWidget>(visualizer));
			AddWidget(std::make_shared<EffectWidget>(visualizer));
			AddWidget(std::make_shared<RenderWidget>(visualizer));
			AddWidget(std::make_shared<LightingWidget>(visualizer));
			AddWidget(std::make_shared<AudioWidget>(visualizer));
			AddWidget(std::make_shared<SystemWidget>(visualizer, scene_manager));
			AddWidget(std::make_shared<MapWidget>(visualizer));
			AddWidget(std::make_shared<ProfilerWidget>());
			AddWidget(std::make_shared<SpaceProbeWidget>(visualizer));

			// Register Space Probe HUD element
			hud_manager.AddElement(std::make_shared<HudSpaceProbe>(visualizer));
		}

		void UIConfigManager::Render() {
			PROJECT_PROFILE_SCOPE("UIConfigManager::Render");

			bool any_hud_visible = std::any_of(m_widgets.begin(), m_widgets.end(), [](const auto& widget) {
				return widget->IsHud() && widget->IsVisible();
			});

			if (!m_show_menus && !any_hud_visible) {
				return;
			}

			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			if (m_show_menus) {
				if (ImGui::BeginMainMenuBar()) {
					if (ImGui::BeginMenu("Windows")) {
						auto widget_env = GetWidget<EnvironmentWidget>();
						if (widget_env) {
							bool visible = widget_env->IsVisible();
							if (ImGui::MenuItem("Environment", nullptr, &visible)) {
								widget_env->SetVisible(visible);
							}
						}

						auto widget_map = GetWidget<MapWidget>();
						if (widget_map) {
							bool visible = widget_map->IsVisible();
							if (ImGui::MenuItem("Environmental Maps", nullptr, &visible)) {
								widget_map->SetVisible(visible);
							}
						}

						auto widget_mood = GetWidget<MoodWidget>();
						if (widget_mood) {
							bool visible = widget_mood->IsVisible();
							if (ImGui::MenuItem("Mood Engine", nullptr, &visible)) {
								widget_mood->SetVisible(visible);
							}
						}

						auto widget_light = GetWidget<LightningWidget>();
						if (widget_light) {
							bool visible = widget_light->IsVisible();
							if (ImGui::MenuItem("Lightning Control", nullptr, &visible)) {
								widget_light->SetVisible(visible);
							}
						}

						auto widget_eff = GetWidget<EffectWidget>();
						if (widget_eff) {
							bool visible = widget_eff->IsVisible();
							if (ImGui::MenuItem("Effects", nullptr, &visible)) {
								widget_eff->SetVisible(visible);
							}
						}

						auto widget_rend = GetWidget<RenderWidget>();
						if (widget_rend) {
							bool visible = widget_rend->IsVisible();
							if (ImGui::MenuItem("Render Settings", nullptr, &visible)) {
								widget_rend->SetVisible(visible);
							}
						}

						auto widget_ltg = GetWidget<LightingWidget>();
						if (widget_ltg) {
							bool visible = widget_ltg->IsVisible();
							if (ImGui::MenuItem("Lighting", nullptr, &visible)) {
								widget_ltg->SetVisible(visible);
							}
						}

						auto widget_aud = GetWidget<AudioWidget>();
						if (widget_aud) {
							bool visible = widget_aud->IsVisible();
							if (ImGui::MenuItem("Audio Controls", nullptr, &visible)) {
								widget_aud->SetVisible(visible);
							}
						}

						auto widget_sys = GetWidget<SystemWidget>();
						if (widget_sys) {
							bool visible = widget_sys->IsVisible();
							if (ImGui::MenuItem("System", nullptr, &visible)) {
								widget_sys->SetVisible(visible);
							}
						}

						auto widget_prof = GetWidget<ProfilerWidget>();
						if (widget_prof) {
							bool visible = widget_prof->IsVisible();
							if (ImGui::MenuItem("Profiler", nullptr, &visible)) {
								widget_prof->SetVisible(visible);
							}
						}

						auto widget_probe = GetWidget<SpaceProbeWidget>();
						if (widget_probe) {
							bool visible = widget_probe->IsVisible();
							if (ImGui::MenuItem("Space Probe", nullptr, &visible)) {
								widget_probe->SetVisible(visible);
							}
						}

						ImGui::EndMenu();
					}

					ImGui::EndMainMenuBar();
				}
			}

			for (const auto& widget : m_widgets) {
				if (widget->IsHud() || m_show_menus) {
					widget->Draw();
				}
			}

			// Render the floating Quick Controls toolbar if menus are open
			if (m_show_menus && m_visualizer) {
				ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f - 175.0f, 20.0f), ImGuiCond_FirstUseEver);
				ImGui::SetNextWindowSize(ImVec2(350, 150), ImGuiCond_FirstUseEver);
				if (ImGui::Begin("Quick Controls", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
					auto& cycle = m_visualizer->GetLightManager().GetDayNightCycle();
					ImGui::Text("Day/Night Cycle:");
					ImGui::SliderFloat("Time (24h)##Quick", &cycle.time, 0.0f, 24.0f, "%.1f h");
					ImGui::Checkbox("Pause Day/Night Cycle##Quick", &cycle.paused);

					ImGui::Separator();

					auto mood_mgr = m_visualizer->GetMoodManager();
					if (mood_mgr) {
						bool mood_enabled = mood_mgr->IsEnabled();
						if (ImGui::Checkbox("Enable Mood Engine##Quick", &mood_enabled)) {
							mood_mgr->SetEnabled(mood_enabled);
						}
					}
				}
				ImGui::End();
			}

			PositionMinimizedWindows();

			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		}

		void UIConfigManager::PositionMinimizedWindows() {
			ImGuiContext& g = *GImGui;
			ImGuiIO&      io = ImGui::GetIO();

			const float padding = 10.0f;
			const float uniform_width = 250.0f;
			float       current_y = padding;

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
						// Just collapsed, save current position and size
						state.last_expanded_pos = window->Pos;
						state.last_expanded_size = window->SizeFull;
						state.was_collapsed = true;
					}
					collapsed_windows.push_back(window);
				} else {
					if (state.was_collapsed) {
						// Just uncollapsed, restore previous size
						window->SizeFull = state.last_expanded_size;

						// Right-justify upon opening to keep it on screen
						window->Pos = ImVec2(io.DisplaySize.x - window->SizeFull.x - padding, padding);

						state.was_collapsed = false;
					} else {
						// Update expanded position as user moves it
						state.last_expanded_pos = window->Pos;
						state.last_expanded_size = window->SizeFull;
					}
				}
			}

			// Sort collapsed windows by ID for stable positioning
			std::sort(collapsed_windows.begin(), collapsed_windows.end(), [](ImGuiWindow* a, ImGuiWindow* b) {
				return a->ID < b->ID;
			});

			for (ImGuiWindow* window : collapsed_windows) {
				// Position in top-right corner, uniform width, right justified
				window->SizeFull.x = uniform_width;
				window->Pos = ImVec2(io.DisplaySize.x - uniform_width - padding, current_y);
				current_y += window->TitleBarHeight + 5.0f; // Padding
			}
		}
	} // namespace UI
} // namespace Boidsish
