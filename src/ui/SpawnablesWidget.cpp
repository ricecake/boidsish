#include "ui/SpawnablesWidget.h"
#include "graphics.h"
#include "hot_air_balloon.h"
#include "imgui.h"
#include <algorithm>

namespace Boidsish {
	namespace UI {

		SpawnablesWidget::SpawnablesWidget(Visualizer& visualizer) : m_visualizer(visualizer) {
			// Lazy initialize m_entity_handler and update handler in Draw()
			// because the Visualizer is still under construction at this point.
		}

		SpawnablesWidget::~SpawnablesWidget() {
			// Clean up any remaining entities
			if (m_entity_handler) {
				auto balloons = m_entity_handler->GetEntitiesByType<HotAirBalloonEntity>();
				for (const auto& balloon : balloons) {
					m_entity_handler->RemoveEntity(balloon->GetId());
				}
			}
		}

		void SpawnablesWidget::SpawnBalloon() {
			if (!m_entity_handler) return;

			int id = m_entity_handler->AddEntity<HotAirBalloonEntity>(
				m_settings.x,
				m_settings.y,
				m_settings.z,
				m_settings.size,
				glm::vec4(m_settings.color[0], m_settings.color[1], m_settings.color[2], m_settings.color[3]),
				m_settings.buoyancy,
				m_settings.wind_response
			);

			if (auto balloon = std::dynamic_pointer_cast<HotAirBalloonEntity>(m_entity_handler->GetEntity(id))) {
				balloon->SetName(m_settings.name);
				balloon->SetSpringStiffness(m_settings.stiffness);
				balloon->SetSpringDamping(m_settings.damping);
			}
		}

		void SpawnablesWidget::Draw() {
			if (!m_show) return;

			if (m_first_draw) {
				// Wrap visualizer in a shared_ptr with a no-op deleter
				auto vis_ptr = std::shared_ptr<Visualizer>(&m_visualizer, [](Visualizer*) {});
				m_entity_handler = std::make_unique<EntityHandler>(m_visualizer.GetThreadPool(), vis_ptr);

				// Register update handler with visualizer to run entity updates every frame
				m_visualizer.AddUpdateHandler([this](float dt, float total_time) {
					if (m_entity_handler) {
						(*m_entity_handler)(total_time);
					}
				});

				// Position in front of the camera on first open
				auto cam_pos = m_visualizer.GetCamera().pos();
				auto cam_front = m_visualizer.GetCamera().front();
				glm::vec3 spawn_pos = cam_pos + cam_front * 30.0f;

				// Make sure we're above terrain
				float terrain_h = 0.0f;
				auto [h, norm] = m_visualizer.GetTerrainPropertiesAtPoint(spawn_pos.x, spawn_pos.z);
				terrain_h = h;
				spawn_pos.y = std::max(spawn_pos.y, terrain_h + 10.0f);

				m_settings.x = spawn_pos.x;
				m_settings.y = spawn_pos.y;
				m_settings.z = spawn_pos.z;
				m_first_draw = false;
			}

			ImGui::SetNextWindowPos(ImVec2(20, 300), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_FirstUseEver);

			if (ImGui::Begin("Spawnables", &m_show)) {
				if (ImGui::CollapsingHeader("Spawn Hot Air Balloon", ImGuiTreeNodeFlags_DefaultOpen)) {
					ImGui::InputText("Instance Name", m_settings.name, sizeof(m_settings.name));

					ImGui::Text("Spawn Position:");
					ImGui::DragFloat("X##Spawn", &m_settings.x, 1.0f);
					ImGui::DragFloat("Y##Spawn", &m_settings.y, 1.0f, 0.0f, 1000.0f);
					ImGui::DragFloat("Z##Spawn", &m_settings.z, 1.0f);

					if (ImGui::Button("Spawn At Camera Position")) {
						auto cam_pos = m_visualizer.GetCamera().pos();
						auto cam_front = m_visualizer.GetCamera().front();
						glm::vec3 spawn_pos = cam_pos + cam_front * 30.0f;

						float terrain_h = 0.0f;
						auto [h, norm] = m_visualizer.GetTerrainPropertiesAtPoint(spawn_pos.x, spawn_pos.z);
						terrain_h = h;
						spawn_pos.y = std::max(spawn_pos.y, terrain_h + 10.0f);

						m_settings.x = spawn_pos.x;
						m_settings.y = spawn_pos.y;
						m_settings.z = spawn_pos.z;
					}

					ImGui::Separator();

					ImGui::SliderFloat("Balloon Radius", &m_settings.size, 1.0f, 30.0f, "%.1f");
					ImGui::ColorEdit4("Envelope Color", m_settings.color);
					ImGui::SliderFloat("Buoyancy / Lift", &m_settings.buoyancy, 0.0f, 30.0f, "%.1f");
					ImGui::SliderFloat("Wind Response", &m_settings.wind_response, 0.0f, 5.0f, "%.1f");
					ImGui::SliderFloat("Spring Stiffness", &m_settings.stiffness, 5.0f, 100.0f, "%.1f");
					ImGui::SliderFloat("Spring Damping", &m_settings.damping, 0.1f, 10.0f, "%.1f");

					ImGui::Spacing();
					if (ImGui::Button("Spawn Balloon", ImVec2(-1, 0))) {
						SpawnBalloon();
					}
				}

				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();

				if (ImGui::CollapsingHeader("Active Spawned Instances", ImGuiTreeNodeFlags_DefaultOpen)) {
					auto balloons = m_entity_handler ? m_entity_handler->GetEntitiesByType<HotAirBalloonEntity>() : std::vector<std::shared_ptr<HotAirBalloonEntity>>();
					if (balloons.empty()) {
						ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No active instances spawned.");
					} else {
						for (size_t i = 0; i < balloons.size(); ++i) {
							auto& balloon = balloons[i];
							std::string header_label = balloon->GetName() + " (ID: " + std::to_string(balloon->GetId()) + ")";

							ImGui::PushID(balloon->GetId());

							if (ImGui::TreeNode(header_label.c_str())) {
								glm::vec3 pos = balloon->GetPosition().Toglm();
								ImGui::Text("Position: X: %.1f, Y: %.1f, Z: %.1f", pos.x, pos.y, pos.z);

								if (ImGui::Button("Focus (Chase)")) {
									m_visualizer.SetChaseCamera(balloon);
								}
								ImGui::SameLine();
								if (ImGui::Button("Focus (Look At)")) {
									auto cam_pos = m_visualizer.GetCamera().pos();
									glm::vec3 offset(0.0f, 10.0f, 30.0f);
									glm::vec3 desired_cam = pos + offset;

									Camera cam = m_visualizer.GetCamera();
									cam.x = desired_cam.x;
									cam.y = desired_cam.y;
									cam.z = desired_cam.z;
									m_visualizer.SetCamera(cam);
									m_visualizer.LookAt(pos);
								}
								ImGui::SameLine();
								if (ImGui::Button("Delete")) {
									m_entity_handler->RemoveEntity(balloon->GetId());
									ImGui::TreePop();
									ImGui::PopID();
									break; // Break loop because list was modified
								}

								ImGui::Separator();

								// Settings modification
								float buoyancy = balloon->GetBuoyancy();
								if (ImGui::SliderFloat("Buoyancy / Lift##Mod", &buoyancy, 0.0f, 30.0f, "%.1f")) {
									balloon->SetBuoyancy(buoyancy);
								}

								float wind_resp = balloon->GetWindResponse();
								if (ImGui::SliderFloat("Wind Response##Mod", &wind_resp, 0.0f, 5.0f, "%.1f")) {
									balloon->SetWindResponse(wind_resp);
								}

								float size = balloon->GetBalloonSize();
								if (ImGui::SliderFloat("Size##Mod", &size, 1.0f, 30.0f, "%.1f")) {
									balloon->SetBalloonSize(size);
								}

								glm::vec4 color = balloon->GetBalloonColor();
								float color_arr[4] = { color.r, color.g, color.b, color.a };
								if (ImGui::ColorEdit4("Color##Mod", color_arr)) {
									balloon->SetBalloonColor(glm::vec4(color_arr[0], color_arr[1], color_arr[2], color_arr[3]));
								}

								float stiffness = balloon->GetSpringStiffness();
								if (ImGui::SliderFloat("Stiffness##Mod", &stiffness, 5.0f, 100.0f, "%.1f")) {
									balloon->SetSpringStiffness(stiffness);
								}

								float damping = balloon->GetSpringDamping();
								if (ImGui::SliderFloat("Damping##Mod", &damping, 0.1f, 10.0f, "%.1f")) {
									balloon->SetSpringDamping(damping);
								}

								ImGui::TreePop();
							}

							ImGui::PopID();
						}
					}
				}
			}
			ImGui::End();
		}

	} // namespace UI
} // namespace Boidsish
