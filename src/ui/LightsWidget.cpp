#include "ui/LightsWidget.h"

#include "imgui.h"
#include <cstring>

namespace Boidsish {
	namespace UI {

		LightsWidget::LightsWidget(LightManager& lightManager): _lightManager(lightManager) {
			m_show = true;
		}

		void LightsWidget::Draw() {
			if (!m_show) {
				return;
			}

			ImGui::Begin("Lights", &m_show);

			// Ambient Light
			glm::vec3 ambient = _lightManager.GetAmbientLight();
			if (ImGui::ColorEdit3("Ambient Light", &ambient[0])) {
				_lightManager.SetAmbientLight(ambient);
			}

			ImGui::Separator();

			int light_to_remove = -1;
			// Lights
			auto& lights = _lightManager.GetLights();
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
					ImGui::DragFloat3("Position", &lights[i].position[0], 0.1f);

					// Direction
					if (lights[i].type != POINT_LIGHT) {
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
					const char* behaviors[] = {"None", "Blink", "Pulse", "Ease In", "Ease Out", "Ease In Out", "Flicker", "Morse"};
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
							ImGui::DragFloat("Flicker Intensity", &lights[i].behavior.flicker_intensity, 0.1f, 0.0f, 5.0f);
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
				_lightManager.AddLight(Light::Create({0, 10, 0}, 5.0f, {1, 1, 1}, false));
			}

			ImGui::End();
		}

	} // namespace UI
} // namespace Boidsish
