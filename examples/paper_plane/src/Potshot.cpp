#include "Potshot.h"

#include <cmath>
#include "PaperPlane.h"
#include "PaperPlaneHandler.h"
#include "Tracer.h"
#include "graphics.h"
#include <glm/gtx/quaternion.hpp>
#include <random>

namespace Boidsish {

	Potshot::Potshot(int id, Vector3 pos): Entity<Model>(id, "assets/smolbird.fbx", true) {
		SetPosition(pos);
		shape_->SetScale(glm::vec3(0.2f));
		SetColor(0.2f, 0.8f, 0.2f); // Greenish
		shape_->SetInstanced(true);
		SetOrientToVelocity(true);
	}

	void Potshot::PickNewPosition(const glm::vec3& player_forward) {
		std::random_device                    rd;
		std::mt19937                          gen(rd());
		std::uniform_real_distribution<float> dist_range(120.0f, 250.0f);
		std::uniform_real_distribution<float> angle_range(-0.4f, 0.4f);

		float d = dist_range(gen);
		float ax = angle_range(gen);
		float ay = angle_range(gen);

		glm::vec3 up(0, 1, 0);
		glm::vec3 right = glm::normalize(glm::cross(player_forward, up));
		if (glm::length(right) < 0.001f)
			right = glm::vec3(1, 0, 0);
		glm::vec3 actual_up = glm::cross(right, player_forward);

		relative_target_pos_ = player_forward * d + right * (ax * d) + actual_up * (ay * d * 0.4f);
	}

	void Potshot::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		(void)time;
		lived_ += delta_time;
		if (lived_ > lifetime_) {
			handler.QueueRemoveEntity(id_);
			return;
		}

		auto planes = handler.GetEntitiesByType<PaperPlane>();
		if (planes.empty())
			return;

		auto      plane = planes[0];
		glm::vec3 player_pos = plane->GetPosition().Toglm();
		glm::vec3 player_forward = plane->GetOrientation() * glm::vec3(0, 0, -1);
		glm::vec3 player_vel = plane->GetVelocity().Toglm();

		if (!initialized_target_) {
			PickNewPosition(player_forward);
			initialized_target_ = true;
			reposition_timer_ = 2.0f;
		}

		reposition_timer_ -= delta_time;
		if (reposition_timer_ <= 0) {
			PickNewPosition(player_forward);
			reposition_timer_ = 2.5f;
			shots_to_fire_ = 2;
			fire_timer_ = 0.4f;
		}

		glm::vec3 world_target_pos = player_pos + relative_target_pos_;
		glm::vec3 current_pos = GetPosition().Toglm();
		glm::vec3 to_target = world_target_pos - current_pos;
		float     dist_to_target = glm::length(to_target);

		if (dist_to_target > 10.0f) {
			glm::vec3 move_dir = glm::normalize(to_target);
			glm::vec3 new_vel = move_dir * speed_;
			SetVelocity(Vector3(new_vel.x, new_vel.y, new_vel.z));
		} else {
			SetVelocity(Vector3(player_vel.x, player_vel.y, player_vel.z));
		}

		if (shots_to_fire_ > 0) {
			fire_timer_ -= delta_time;
			if (fire_timer_ <= 0) {
				shots_to_fire_--;
				fire_timer_ = 0.3f;

				glm::vec3 to_player = player_pos - current_pos;
				float     dist_to_player = glm::length(to_player);
				if (dist_to_player > 0.001f) {
					glm::vec3 fire_dir = glm::normalize(to_player);
					// Lead target slightly
					float     bullet_speed = 500.0f;
					float     time_to_impact = dist_to_player / bullet_speed;
					glm::vec3 lead_pos = player_pos + player_vel * time_to_impact;
					fire_dir = glm::normalize(lead_pos - current_pos);

					handler.QueueAddEntity<Tracer>(
						current_pos + fire_dir * 3.0f,
						GetOrientation(),
						fire_dir * bullet_speed,
						glm::vec3(0.2f, 1.0f, 0.2f)
					);
				}
			}
		}

		// Terrain avoidance
		auto [h, norm] = handler.GetTerrainPropertiesAtPoint(current_pos.x, current_pos.z);
		if (current_pos.y < h + 20.0f) {
			rigid_body_.AddForce(glm::vec3(0, 100.0f, 0));
		}
	}

	void Potshot::OnHit(const EntityHandler& handler, float damage) {
		health_ -= damage;
		if (health_ <= 0) {
			auto pos = GetPosition().Toglm();
			handler.EnqueueVisualizerAction([pos, &handler, this]() {
				handler.vis->CreateExplosion(pos, 0.8f);
				handler.QueueRemoveEntity(this->id_);
			});
			if (auto* pp_handler = dynamic_cast<const PaperPlaneHandler*>(&handler)) {
				pp_handler->AddScore(300, "Potshot Destroyed");
			}
		}
	}

} // namespace Boidsish
