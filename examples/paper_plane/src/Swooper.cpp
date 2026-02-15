#include "Swooper.h"

#include <cmath>
#include "PaperPlane.h"
#include "PaperPlaneHandler.h"
#include "Tracer.h"
#include "graphics.h"
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

	Swooper::Swooper(int id, Vector3 pos): Entity<Model>(id, "assets/dogplane.obj", true) {
		SetPosition(pos);
		shape_->SetScale(glm::vec3(0.5f));
		SetColor(0.2f, 0.2f, 0.8f); // Bluish
		shape_->SetInstanced(true);
		SetOrientToVelocity(true);

		std::dynamic_pointer_cast<Model>(shape_)->SetBaseRotation(
			glm::angleAxis(glm::radians(-180.0f), glm::vec3(0.0f, 1.0f, 0.0f))
		);
	}

	void Swooper::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		(void)time;
		auto planes = handler.GetEntitiesByType<PaperPlane>();
		if (planes.empty())
			return;

		auto      plane = planes[0];
		glm::vec3 player_pos = plane->GetPosition().Toglm();
		glm::vec3 player_forward = plane->GetOrientation() * glm::vec3(0, 0, -1);
		glm::vec3 current_pos = GetPosition().Toglm();

		glm::vec3 to_player = player_pos - current_pos;
		float     dist = glm::length(to_player);

		glm::vec3 to_enemy = current_pos - player_pos;
		bool      is_behind = false;
		if (glm::length(to_enemy) > 0.001f) {
			float dot_forward = glm::dot(player_forward, glm::normalize(to_enemy));
			is_behind = dot_forward < -0.1f;
		}

		if (dist < 0.001f)
			return;

		glm::vec3 dir = glm::normalize(to_player);

		float current_speed = swooping_ ? speed_ * 1.8f : speed_;

		if (is_behind) {
			current_speed *= 3.5f; // Catch up fast
			glm::vec3 target_pos = player_pos + player_forward * 150.0f;
			glm::vec3 move_dir = glm::normalize(target_pos - current_pos);
			glm::vec3 new_vel = move_dir * current_speed;
			SetVelocity(Vector3(new_vel.x, new_vel.y, new_vel.z));
		} else {
			if (dist < 250.0f && !swooping_) {
				swooping_ = true;
				zigzag_amplitude_ *= 2.5f;
				zigzag_speed_ *= 2.0f;
			}

			zigzag_phase_ += zigzag_speed_ * delta_time;
			zigzag_amplitude_ += 8.0f * delta_time; // Broadening

			glm::vec3 up(0, 1, 0);
			glm::vec3 right = glm::normalize(glm::cross(dir, up));
			if (glm::length(right) < 0.001f)
				right = glm::vec3(1, 0, 0);
			glm::vec3 actual_up = glm::cross(right, dir);

			// Zigzag in a horizontal plane mostly, but with some vertical
			glm::vec3 offset = right * std::sin(zigzag_phase_) * zigzag_amplitude_ +
							   actual_up * std::cos(zigzag_phase_ * 0.5f) * (zigzag_amplitude_ * 0.3f);

			glm::vec3 desired_pos = player_pos + offset;
			glm::vec3 move_dir = glm::normalize(desired_pos - current_pos);

			glm::vec3 new_vel = move_dir * current_speed;
			SetVelocity(Vector3(new_vel.x, new_vel.y, new_vel.z));
		}

		// Firing logic
		time_to_fire_ -= delta_time;
		if (!is_behind && time_to_fire_ <= 0 && dist < 800.0f && dist > 50.0f) {
			float fire_rate = swooping_ ? 0.15f : 0.8f;
			time_to_fire_ = fire_rate;

			glm::vec3 fire_dir = glm::normalize(player_pos - current_pos);
			// Lead target slightly
			glm::vec3 player_vel = plane->GetVelocity().Toglm();
			float     bullet_speed = 400.0f;
			float     time_to_impact = dist / bullet_speed;
			glm::vec3 lead_pos = player_pos + player_vel * time_to_impact;
			fire_dir = glm::normalize(lead_pos - current_pos);

			handler.QueueAddEntity<Tracer>(
				current_pos + fire_dir * 5.0f,
				GetOrientation(),
				fire_dir * bullet_speed,
				glm::vec3(0.2f, 0.2f, 1.0f)
			);
		}

		// Terrain avoidance/snap
		auto [h, norm] = handler.GetTerrainPropertiesAtPoint(current_pos.x, current_pos.z);
		if (current_pos.y < h + 2.0f) {
			SetPosition(current_pos.x, h + 2.0f, current_pos.z);
			rigid_body_.AddForce(glm::vec3(0, 200.0f, 0));
			glm::vec3 vel = GetVelocity().Toglm();
			if (vel.y < 0) {
				vel.y = -vel.y * 0.5f;
				SetVelocity(Vector3(vel.x, vel.y, vel.z));
			}
		} else if (current_pos.y < h + 15.0f) {
			rigid_body_.AddForce(glm::vec3(0, 80.0f, 0));
		}

		// If we passed the player, eventually remove
		if (swooping_ && glm::dot(dir, glm::vec3(0, 0, -1)) < -0.5f && dist > 500.0f) {
			handler.QueueRemoveEntity(id_);
		}
	}

	void Swooper::OnHit(const EntityHandler& handler, float damage) {
		health_ -= damage;
		if (health_ <= 0) {
			auto pos = GetPosition().Toglm();
			handler.EnqueueVisualizerAction([pos, &handler, this]() {
				handler.vis->CreateExplosion(pos, 1.5f);
				handler.QueueRemoveEntity(this->id_);
			});
			if (auto* pp_handler = dynamic_cast<const PaperPlaneHandler*>(&handler)) {
				pp_handler->AddScore(400, "Swooper Destroyed");
			}
		}
	}

} // namespace Boidsish
