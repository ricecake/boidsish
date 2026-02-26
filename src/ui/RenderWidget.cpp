#include "ui/RenderWidget.h"

#include "ConfigManager.h"
#include "graphics.h"
#include "imgui.h"
#include "light_manager.h"

namespace Boidsish {
	namespace UI {

		RenderWidget::RenderWidget(Visualizer& visualizer): m_visualizer(visualizer) {}

		void RenderWidget::Draw() {
			if (!m_show)
				return;

			ImGui::SetNextWindowPos(ImVec2(20, 640), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("Render", &m_show)) {
				// 1. Rendering Settings (from ConfigWidget)
				if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
					float render_scale = m_visualizer.GetRenderScale();
					if (ImGui::SliderFloat("Render Scale", &render_scale, 0.1f, 1.0f)) {
						m_visualizer.SetRenderScale(render_scale);
					}

					auto& config_manager = ConfigManager::GetInstance();
					bool  enable_shadows = config_manager.GetAppSettingBool("enable_shadows", true);
					if (ImGui::Checkbox("Enable Shadows", &enable_shadows)) {
						config_manager.SetBool("enable_shadows", enable_shadows);
					}
				}

				// 2. Mesh Optimization (from ConfigWidget)
				if (ImGui::CollapsingHeader("Mesh Optimization", ImGuiTreeNodeFlags_DefaultOpen)) {
					auto& config_manager = ConfigManager::GetInstance();

					bool mesh_opt = config_manager.GetAppSettingBool("mesh_optimizer_enabled", true);
					if (ImGui::Checkbox("Enable Optimizer", &mesh_opt)) {
						config_manager.SetBool("mesh_optimizer_enabled", mesh_opt);
					}

					bool mesh_simp = config_manager.GetAppSettingBool("mesh_simplifier_enabled", false);
					if (ImGui::Checkbox("Enable Simplifier", &mesh_simp)) {
						config_manager.SetBool("mesh_simplifier_enabled", mesh_simp);
					}

					if (mesh_simp) {
						float ratio = config_manager.GetAppSettingFloat("mesh_simplifier_target_ratio", 0.5f);
						if (ImGui::SliderFloat("Global Ratio Limit", &ratio, 0.01f, 1.0f)) {
							config_manager.SetFloat("mesh_simplifier_target_ratio", ratio);
						}

						ImGui::Separator();
						ImGui::Text("Pre-build Assets");
						float err_pb = config_manager.GetAppSettingFloat("mesh_simplifier_error_prebuild", 0.01f);
						if (ImGui::SliderFloat("Error Rate (Pre-build)", &err_pb, 0.001f, 0.2f, "%.3f")) {
							config_manager.SetFloat("mesh_simplifier_error_prebuild", err_pb);
						}

						int         agg_pb = config_manager.GetAppSettingInt("mesh_simplifier_aggression_prebuild", 0);
						const char* agg_levels[] = {"Low", "Medium", "High"};
						int         current_agg_pb = (agg_pb == 40) ? 2 : (agg_pb == 8 ? 1 : 0);
						if (ImGui::Combo("Aggression (Pre-build)", &current_agg_pb, agg_levels, 3)) {
							int new_agg = (current_agg_pb == 2) ? 40 : (current_agg_pb == 1 ? 8 : 0);
							config_manager.SetInt("mesh_simplifier_aggression_prebuild", new_agg);
						}

						ImGui::Separator();
						ImGui::Text("Procedural Models");
						float err_proc = config_manager.GetAppSettingFloat("mesh_simplifier_error_procedural", 0.05f);
						if (ImGui::SliderFloat("Error Rate (Procedural)", &err_proc, 0.001f, 0.2f, "%.3f")) {
							config_manager.SetFloat("mesh_simplifier_error_procedural", err_proc);
						}

						int agg_proc = config_manager.GetAppSettingInt("mesh_simplifier_aggression_procedural", 40);
						int current_agg_proc = (agg_proc == 40) ? 2 : (agg_proc == 8 ? 1 : 0);
						if (ImGui::Combo("Aggression (Procedural)", &current_agg_proc, agg_levels, 3)) {
							int new_agg = (current_agg_proc == 2) ? 40 : (current_agg_proc == 1 ? 8 : 0);
							config_manager.SetInt("mesh_simplifier_aggression_procedural", new_agg);
						}
					}
				}

				// 3. Ambient & Individual Lights (from LightsWidget)
				if (ImGui::CollapsingHeader("Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
					auto& light_manager = m_visualizer.GetLightManager();

					// Ambient Light
					glm::vec3 ambient = light_manager.GetAmbientLight();
					if (ImGui::ColorEdit3("Ambient Light", &ambient[0])) {
						light_manager.SetAmbientLight(ambient);
					}

					ImGui::Separator();

					int   light_to_remove = -1;
					auto& lights = light_manager.GetLights();
					for (int i = 0; i < lights.size(); ++i) {
						ImGui::PushID(i);
						if (ImGui::TreeNode("Light", "Light %d", i)) {
							// Type
							const char* types[] = {"Point", "Directional", "Spot"};
							int         current_type = lights[i].type;
							if (ImGui::Combo("Type", &current_type, types, IM_ARRAYSIZE(types))) {
								lights[i].type = (LightType)current_type;
							}

							// Position
							if (lights[i].type != DIRECTIONAL_LIGHT) {
								ImGui::DragFloat3("Position", &lights[i].position[0], 0.1f);
							}

							// Direction / Angles
							if (lights[i].type == DIRECTIONAL_LIGHT) {
								bool changed = false;
								changed |= ImGui::SliderFloat("Azimuth", &lights[i].azimuth, 0.0f, 360.0f);
								changed |= ImGui::SliderFloat("Elevation", &lights[i].elevation, -90.0f, 90.0f);
								if (changed) {
									lights[i].UpdateDirectionFromAngles();
								}
							} else if (lights[i].type == SPOT_LIGHT) {
								ImGui::DragFloat3("Direction", &lights[i].direction[0], 0.1f);
							}

							// Color
							ImGui::ColorEdit3("Color", &lights[i].color[0]);

							// Intensity
							if (ImGui::DragFloat("Intensity", &lights[i].base_intensity, 0.1f)) {
								if (lights[i].behavior.type == LightBehaviorType::NONE) {
									lights[i].intensity = lights[i].base_intensity;
								}
							}

							// Behavior
							const char* behaviors[] =
								{"None", "Blink", "Pulse", "Ease In", "Ease Out", "Ease In Out", "Flicker", "Morse"};
							int current_behavior = (int)lights[i].behavior.type;
							if (ImGui::Combo("Behavior", &current_behavior, behaviors, IM_ARRAYSIZE(behaviors))) {
								lights[i].behavior.type = (LightBehaviorType)current_behavior;
								lights[i].behavior.timer = 0.0f;
								if (lights[i].behavior.type == LightBehaviorType::MORSE) {
									lights[i].behavior.morse_index = -1; // Trigger regeneration
								}
							}

							if (lights[i].behavior.type != LightBehaviorType::NONE) {
								ImGui::Indent();
								if (lights[i].behavior.type == LightBehaviorType::BLINK) {
									ImGui::DragFloat("Period", &lights[i].behavior.period, 0.1f, 0.1f, 10.0f);
									ImGui::DragFloat("Duty Cycle", &lights[i].behavior.duty_cycle, 0.01f, 0.0f, 1.0f);
								} else if (lights[i].behavior.type == LightBehaviorType::PULSE) {
									ImGui::DragFloat("Period", &lights[i].behavior.period, 0.1f, 0.1f, 10.0f);
									ImGui::DragFloat("Amplitude", &lights[i].behavior.amplitude, 0.01f, 0.0f, 1.0f);
								} else if (lights[i].behavior.type == LightBehaviorType::EASE_IN ||
								           lights[i].behavior.type == LightBehaviorType::EASE_OUT ||
								           lights[i].behavior.type == LightBehaviorType::EASE_IN_OUT) {
									ImGui::DragFloat("Duration", &lights[i].behavior.period, 0.1f, 0.1f, 10.0f);
								} else if (lights[i].behavior.type == LightBehaviorType::FLICKER) {
									ImGui::DragFloat(
										"Flicker Intensity",
										&lights[i].behavior.flicker_intensity,
										0.1f,
										0.0f,
										5.0f
									);
								} else if (lights[i].behavior.type == LightBehaviorType::MORSE) {
									char msg_buf[128];
									strncpy(msg_buf, lights[i].behavior.message.c_str(), sizeof(msg_buf));
									msg_buf[sizeof(msg_buf) - 1] = '\0';
									if (ImGui::InputText("Message", msg_buf, sizeof(msg_buf))) {
										lights[i].behavior.message = msg_buf;
										lights[i].behavior.morse_index = -1;
									}
									ImGui::DragFloat("Unit Time", &lights[i].behavior.period, 0.01f, 0.01f, 1.0f);
									ImGui::Checkbox("Loop", &lights[i].behavior.loop);
								}
								ImGui::Unindent();
							}

							// Cutoffs
							if (lights[i].type == SPOT_LIGHT) {
								float inner_angle = glm::degrees(glm::acos(lights[i].inner_cutoff));
								float outer_angle = glm::degrees(glm::acos(lights[i].outer_cutoff));

								if (ImGui::DragFloat("Inner Angle", &inner_angle, 0.1f, 0.0f, 90.0f)) {
									if (inner_angle > outer_angle) {
										inner_angle = outer_angle;
									}
									lights[i].inner_cutoff = glm::cos(glm::radians(inner_angle));
								}
								if (ImGui::DragFloat("Outer Angle", &outer_angle, 0.1f, 0.0f, 90.0f)) {
									if (inner_angle > outer_angle) {
										outer_angle = inner_angle;
									}
									lights[i].outer_cutoff = glm::cos(glm::radians(outer_angle));
								}
							}

							if (lights.size() > 1) {
								if (ImGui::Button("Remove")) {
									light_to_remove = i;
								}
							}

							ImGui::TreePop();
						}
						ImGui::PopID();
					}
					if (light_to_remove != -1) {
						lights.erase(lights.begin() + light_to_remove);
					}

					if (ImGui::Button("Add Light")) {
						light_manager.AddLight(Light::Create({0, 10, 0}, 5.0f, {1, 1, 1}, false));
					}
				}
			}
			ImGui::End();
		}
	} // namespace UI
} // namespace Boidsish
