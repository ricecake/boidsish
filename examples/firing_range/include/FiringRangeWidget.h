#pragma once
#include "IWidget.h"
#include "FiringRangeHandler.h"
#include "imgui.h"
#include "PaperPlane.h"
#include "GuidedMissileLauncher.h"
#include "CatMissile.h"
#include "graphics.h"
#include <string>

namespace Boidsish {
	namespace UI {
		class FiringRangeWidget : public IWidget {
		public:
			FiringRangeWidget(FiringRangeHandler& handler) : handler_(handler) {}

			void Draw() override {
				ImGui::Begin("Firing Range Controls");

				ImGui::Checkbox("Auto-fire", &handler_.auto_fire);

				int type = (int)handler_.auto_fire_type;
				ImGui::RadioButton("Guided Missile", &type, 0); ImGui::SameLine();
				ImGui::RadioButton("Cat Missile", &type, 1);
				handler_.auto_fire_type = (MissileType)type;

				ImGui::SliderFloat("Fire Interval", &handler_.fire_interval, 0.1f, 10.0f);

				if (ImGui::Button("Spawn Launcher")) {
					auto cam_pos = handler_.vis->GetCamera().pos();
					auto [h, n] = handler_.vis->GetTerrainPointPropertiesThreadSafe(cam_pos.x, cam_pos.z);
					handler_.QueueAddEntity<GuidedMissileLauncher>(Vector3(cam_pos.x, h, cam_pos.z), glm::quat(1,0,0,0));
				}
				ImGui::SameLine();
				if (ImGui::Button("Spawn Target")) {
					auto cam_pos = handler_.vis->GetCamera().pos();
					handler_.QueueAddEntity<PaperPlane>(Vector3(cam_pos.x, cam_pos.y, cam_pos.z));
				}

				ImGui::Separator();
				ImGui::Text("Entities:");
				auto entities = handler_.GetAllEntities();
				for (auto const& [id, entity] : entities) {
					std::string label = "Entity " + std::to_string(id);
					if (std::dynamic_pointer_cast<PaperPlane>(entity)) label += " (Target)";
					else if (std::dynamic_pointer_cast<GuidedMissileLauncher>(entity)) label += " (Launcher)";
					else if (std::dynamic_pointer_cast<CatMissile>(entity)) label += " (CatMissile)";
					else continue;

					if (ImGui::TreeNode(label.c_str())) {
						Vector3 pos = entity->GetPosition();
						float p[3] = {pos.x, pos.y, pos.z};
						if (ImGui::DragFloat3("Position", p, 0.1f)) {
							entity->SetPosition(p[0], p[1], p[2]);
							entity->UpdateShape();
						}

						if (auto launcher = std::dynamic_pointer_cast<GuidedMissileLauncher>(entity)) {
							if (ImGui::Button("Fire Guided!")) {
								launcher->Fire(handler_);
							}
							ImGui::SameLine();
							if (ImGui::Button("Fire Cat!")) {
								handler_.QueueAddEntity<CatMissile>(
									launcher->GetPosition(),
									launcher->GetOrientation(),
									glm::vec3(0, 0, -1),
									Vector3(0, 0, 0)
								);
							}
						}

						if (ImGui::Button("Remove")) {
							handler_.QueueRemoveEntity(id);
						}
						ImGui::TreePop();
					}
				}

				ImGui::End();
			}

		private:
			FiringRangeHandler& handler_;
		};
	}
}
