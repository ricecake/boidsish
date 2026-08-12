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
#include "ui/CompetitiveCAWidget.h"
#include "post_processing/PostProcessingManager.h"
#include "post_processing/effects/AtmosphereEffect.h"
#include "post_processing/effects/VolumetricLightingEffect.h"
#include "weather_manager.h"
#include "terrain_generator_interface.h"
#include "ConfigManager.h"
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
			AddWidget(std::make_shared<UI::CompetitiveCAWidget>(visualizer));

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

			if (!m_settings_loaded && m_visualizer) {
				// Load persisted Quick Controls settings
				auto& cfg = ConfigManager::GetInstance();
				auto& cycle = m_visualizer->GetLightManager().GetDayNightCycle();
				cycle.time = cfg.GetAppSettingFloat("quick_day_night_time", cycle.time);
				cycle.paused = cfg.GetAppSettingBool("quick_day_night_paused", cycle.paused);

				auto mood_mgr = m_visualizer->GetMoodManager();
				if (mood_mgr) {
					bool mood_enabled = cfg.GetAppSettingBool("quick_mood_enabled", mood_mgr->IsEnabled());
					mood_mgr->SetEnabled(mood_enabled);
				}

				auto terrain = m_visualizer->GetTerrain();
				if (terrain) {
					float world_scale = cfg.GetAppSettingFloat("terrain_world_scale", terrain->GetWorldScale());
					terrain->SetWorldScale(world_scale);
				}

				if (m_visualizer->HasPostProcessingManager()) {
					auto& ppm = m_visualizer->GetPostProcessingManager();
					std::shared_ptr<PostProcessing::AtmosphereEffect> atmos_effect = nullptr;
					std::shared_ptr<PostProcessing::VolumetricLightingEffect> vol_effect = nullptr;
					for (auto& effect : ppm.GetPreToneMappingEffects()) {
						if (effect->GetName() == "Atmosphere") {
							atmos_effect = std::dynamic_pointer_cast<PostProcessing::AtmosphereEffect>(effect);
						} else if (effect->GetName() == "Volumetric Lighting") {
							vol_effect = std::dynamic_pointer_cast<PostProcessing::VolumetricLightingEffect>(effect);
						}
					}

					if (atmos_effect) {
						bool atmos_enabled = cfg.GetAppSettingBool("enable_atmosphere", true);
						atmos_effect->SetEnabled(atmos_enabled);

						float cloud_density = cfg.GetAppSettingFloat("quick_cloud_density", atmos_effect->GetCloudDensity());
						auto weather = m_visualizer->GetWeatherManager();
						if (weather) {
							weather->SetTarget(WeatherAttribute::CloudDensity, cloud_density);
						} else {
							atmos_effect->SetCloudDensity(cloud_density);
						}

						float haze_density = cfg.GetAppSettingFloat("quick_haze_density", atmos_effect->GetHazeDensity());
						if (weather) {
							weather->SetTarget(WeatherAttribute::HazeDensity, haze_density);
						} else {
							atmos_effect->SetHazeDensity(haze_density);
						}
						atmos_effect->FlushHistory();
					}

					if (vol_effect) {
						bool vol_enabled = cfg.GetAppSettingBool("enable_volumetric_lighting", true);
						vol_effect->SetEnabled(vol_enabled);

						float vol_intensity = cfg.GetAppSettingFloat("volumetric_intensity", vol_effect->GetIntensity());
						vol_effect->SetIntensity(vol_intensity);
						vol_effect->FlushHistory();
					}
				}
				m_settings_loaded = true;
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
								if (visible) {
									ImGui::SetWindowCollapsed("Environment", false);
								}
							}
						}

						auto widget_map = GetWidget<MapWidget>();
						if (widget_map) {
							bool visible = widget_map->IsVisible();
							if (ImGui::MenuItem("Environmental Maps", nullptr, &visible)) {
								widget_map->SetVisible(visible);
								if (visible) {
									ImGui::SetWindowCollapsed("Environmental Maps", false);
								}
							}
						}

						auto widget_mood = GetWidget<MoodWidget>();
						if (widget_mood) {
							bool visible = widget_mood->IsVisible();
							if (ImGui::MenuItem("Mood Engine", nullptr, &visible)) {
								widget_mood->SetVisible(visible);
								if (visible) {
									ImGui::SetWindowCollapsed("Mood Engine", false);
								}
							}
						}

						auto widget_light = GetWidget<LightningWidget>();
						if (widget_light) {
							bool visible = widget_light->IsVisible();
							if (ImGui::MenuItem("Lightning Control", nullptr, &visible)) {
								widget_light->SetVisible(visible);
								if (visible) {
									ImGui::SetWindowCollapsed("Lightning Control", false);
								}
							}
						}

						auto widget_eff = GetWidget<EffectWidget>();
						if (widget_eff) {
							bool visible = widget_eff->IsVisible();
							if (ImGui::MenuItem("Effects", nullptr, &visible)) {
								widget_eff->SetVisible(visible);
								if (visible) {
									ImGui::SetWindowCollapsed("Effects", false);
								}
							}
						}

						auto widget_rend = GetWidget<RenderWidget>();
						if (widget_rend) {
							bool visible = widget_rend->IsVisible();
							if (ImGui::MenuItem("Render Settings", nullptr, &visible)) {
								widget_rend->SetVisible(visible);
								if (visible) {
									ImGui::SetWindowCollapsed("Render", false);
								}
							}
						}

						auto widget_ltg = GetWidget<LightingWidget>();
						if (widget_ltg) {
							bool visible = widget_ltg->IsVisible();
							if (ImGui::MenuItem("Lighting", nullptr, &visible)) {
								widget_ltg->SetVisible(visible);
								if (visible) {
									ImGui::SetWindowCollapsed("Lighting", false);
								}
							}
						}

						auto widget_aud = GetWidget<AudioWidget>();
						if (widget_aud) {
							bool visible = widget_aud->IsVisible();
							if (ImGui::MenuItem("Audio Controls", nullptr, &visible)) {
								widget_aud->SetVisible(visible);
								if (visible) {
									ImGui::SetWindowCollapsed("Audio Controls", false);
								}
							}
						}

						auto widget_sys = GetWidget<SystemWidget>();
						if (widget_sys) {
							bool visible = widget_sys->IsVisible();
							if (ImGui::MenuItem("System", nullptr, &visible)) {
								widget_sys->SetVisible(visible);
								if (visible) {
									ImGui::SetWindowCollapsed("System", false);
								}
							}
						}

						auto widget_prof = GetWidget<ProfilerWidget>();
						if (widget_prof) {
							bool visible = widget_prof->IsVisible();
							if (ImGui::MenuItem("Profiler", nullptr, &visible)) {
								widget_prof->SetVisible(visible);
								if (visible) {
									ImGui::SetWindowCollapsed("Profiler", false);
								}
							}
						}

						auto widget_probe = GetWidget<SpaceProbeWidget>();
						if (widget_probe) {
							bool visible = widget_probe->IsVisible();
							if (ImGui::MenuItem("Space Probe", nullptr, &visible)) {
								widget_probe->SetVisible(visible);
								if (visible) {
									ImGui::SetWindowCollapsed("Space Probe Analyzer", false);
								}
							}
						}

						auto widget_ca = GetWidget<CompetitiveCAWidget>();
						if (widget_ca) {
							bool visible = widget_ca->IsVisible();
							if (ImGui::MenuItem("Competitive Cellular Automata", nullptr, &visible)) {
								widget_ca->SetVisible(visible);
								if (visible) {
									ImGui::SetWindowCollapsed("Competitive Cellular Automata", false);
								}
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

			// Render the floating Quick Controls toolbar
			if (m_show_menus && m_visualizer) {
				ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f - 175.0f, 20.0f), ImGuiCond_FirstUseEver);
				ImGui::SetNextWindowSize(ImVec2(350, 300), ImGuiCond_FirstUseEver);
				if (ImGui::Begin("Quick Controls", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
					auto& cfg = ConfigManager::GetInstance();

					// 1. Day/Night Cycle
					auto& cycle = m_visualizer->GetLightManager().GetDayNightCycle();
					ImGui::TextColored(ImVec4(0, 1, 1, 1), "Day/Night Cycle:");
					if (ImGui::SliderFloat("Time (24h)##Quick", &cycle.time, 0.0f, 24.0f, "%.1f h")) {
						cfg.SetFloat("quick_day_night_time", cycle.time);
					}
					if (ImGui::Checkbox("Pause Day/Night Cycle##Quick", &cycle.paused)) {
						cfg.SetBool("quick_day_night_paused", cycle.paused);
					}

					ImGui::Separator();

					// 2. Mood Engine
					auto mood_mgr = m_visualizer->GetMoodManager();
					if (mood_mgr) {
						bool mood_enabled = mood_mgr->IsEnabled();
						if (ImGui::Checkbox("Enable Mood Engine##Quick", &mood_enabled)) {
							mood_mgr->SetEnabled(mood_enabled);
							cfg.SetBool("quick_mood_enabled", mood_enabled);
						}
					}

					ImGui::Separator();

					// 3. Terrain
					ImGui::TextColored(ImVec4(0, 1, 1, 1), "Terrain:");
					bool render_terrain = cfg.GetAppSettingBool("render_terrain", true);
					if (ImGui::Checkbox("Render Terrain##Quick", &render_terrain)) {
						cfg.SetBool("render_terrain", render_terrain);
					}

					auto terrain = m_visualizer->GetTerrain();
					if (terrain) {
						float world_scale = terrain->GetWorldScale();
						if (ImGui::SliderFloat("World Scale##Quick", &world_scale, 0.1f, 5.0f)) {
							terrain->SetWorldScale(world_scale);
							cfg.SetFloat("terrain_world_scale", world_scale);
						}
					}

					ImGui::Separator();

					// Find Atmosphere and Volumetric Lighting effects
					std::shared_ptr<PostProcessing::AtmosphereEffect> atmos_effect = nullptr;
					std::shared_ptr<PostProcessing::VolumetricLightingEffect> vol_effect = nullptr;
					if (m_visualizer->HasPostProcessingManager()) {
						auto& ppm = m_visualizer->GetPostProcessingManager();
						for (auto& effect : ppm.GetPreToneMappingEffects()) {
							if (effect->GetName() == "Atmosphere") {
								atmos_effect = std::dynamic_pointer_cast<PostProcessing::AtmosphereEffect>(effect);
							} else if (effect->GetName() == "Volumetric Lighting") {
								vol_effect = std::dynamic_pointer_cast<PostProcessing::VolumetricLightingEffect>(effect);
							}
						}
					}

					// 4. Volumetric Lighting
					if (vol_effect) {
						ImGui::TextColored(ImVec4(0, 1, 1, 1), "Volumetric Lighting:");
						bool vol_enabled = vol_effect->IsEnabled();
						if (ImGui::Checkbox("Enable Volumetric Lighting##Quick", &vol_enabled)) {
							vol_effect->SetEnabled(vol_enabled);
							cfg.SetBool("enable_volumetric_lighting", vol_enabled);
						}

						if (vol_enabled) {
							float vol_intensity = vol_effect->GetIntensity();
							if (ImGui::SliderFloat("Volumetric Intensity##Quick", &vol_intensity, 0.0f, 5.0f)) {
								vol_effect->SetIntensity(vol_intensity);
								cfg.SetFloat("volumetric_intensity", vol_intensity);
								vol_effect->FlushHistory();
							}
						}
						ImGui::Separator();
					}

					// 5. Atmosphere
					if (atmos_effect) {
						ImGui::TextColored(ImVec4(0, 1, 1, 1), "Atmosphere:");
						bool atmos_enabled = atmos_effect->IsEnabled();
						if (ImGui::Checkbox("Enable Atmosphere##Quick", &atmos_enabled)) {
							atmos_effect->SetEnabled(atmos_enabled);
							cfg.SetBool("enable_atmosphere", atmos_enabled);
						}

						if (atmos_enabled) {
							float cloud_density = atmos_effect->GetCloudDensity();
							if (ImGui::SliderFloat("Cloud Density##Quick", &cloud_density, 0.0f, 1.0f)) {
								auto weather = m_visualizer->GetWeatherManager();
								if (weather) {
									weather->SetTarget(WeatherAttribute::CloudDensity, cloud_density);
								} else {
									atmos_effect->SetCloudDensity(cloud_density);
								}
								cfg.SetFloat("quick_cloud_density", cloud_density);
								atmos_effect->FlushHistory();
							}

							float haze_density = atmos_effect->GetHazeDensity();
							if (ImGui::SliderFloat("Haze Density##Quick", &haze_density, 0.0f, 5.0f, "%.2f")) {
								auto weather = m_visualizer->GetWeatherManager();
								if (weather) {
									weather->SetTarget(WeatherAttribute::HazeDensity, haze_density);
								} else {
									atmos_effect->SetHazeDensity(haze_density);
								}
								cfg.SetFloat("quick_haze_density", haze_density);
								atmos_effect->FlushHistory();
							}
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
