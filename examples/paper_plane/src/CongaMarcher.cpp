#include "CongaMarcher.h"

#include <cmath>

#include "PaperPlane.h"
#include "PaperPlaneHandler.h"
#include "graphics.h"
#include "spatial_entity_handler.h"
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

	CongaMarcher::CongaMarcher(int id, Vector3 pos, int leader_id):
		Entity<Model>(id, "assets/utah_teapot.obj", true), leader_id_(leader_id) {
		SetPosition(pos);
		shape_->SetScale(glm::vec3(0.5f));
		SetColor(0.8f, 0.2f, 0.2f); // Reddish
		spiral_phase_ = static_cast<float>(id) * 0.7f;
		SetOrientToVelocity(true);
	}

	void CongaMarcher::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		(void)time;
		auto planes = handler.GetEntitiesByType<PaperPlane>();
		if (planes.empty())
			return;

		auto      plane = planes[0];
		glm::vec3 player_pos = plane->GetPosition().Toglm();
		glm::vec3 player_forward = plane->GetOrientation() * glm::vec3(0, 0, -1);
		glm::vec3 target_pos = player_pos;

		glm::vec3 current_pos = GetPosition().Toglm();
		glm::vec3 to_enemy = current_pos - player_pos;
		float     distance = glm::length(to_enemy);
		float     dot_forward = 0.0f;

		if (distance > 0.001f) {
			dot_forward = glm::dot(player_forward, to_enemy / distance);
		}

		// State transition: enter repositioning if behind
		if (dot_forward < -0.1f) {
			repositioning_ = true;
		}
		// Exit repositioning if in front and far enough
		if (repositioning_ && dot_forward > 0.7f && distance > 120.0f) {
			repositioning_ = false;
		}

		float current_speed = speed_;
		if (repositioning_) {
			current_speed *= 3.0f; // Catch up fast
			// Target a point in front of the player
			target_pos = player_pos + player_forward * 180.0f;
		} else if (leader_id_ != -1) {
			auto leader = handler.GetEntity(leader_id_);
			if (leader) {
				// Follow behind leader
				glm::vec3 leader_pos = leader->GetPosition().Toglm();
				glm::vec3 leader_vel = leader->GetVelocity().Toglm();
				if (glm::length(leader_vel) > 0.001f) {
					target_pos = leader_pos - glm::normalize(leader_vel) * 10.0f;
				} else {
					target_pos = leader_pos;
				}
			} else {
				leader_id_ = -1; // Leader is gone, I am the new leader or just follow player
			}
		}

		glm::vec3 to_target = target_pos - current_pos;
		float     dist_to_target = glm::length(to_target);

		if (dist_to_target > 0.001f) {
			glm::vec3 dir = glm::normalize(to_target);

			// Spiral logic
			spiral_phase_ += spiral_speed_ * delta_time;
			glm::vec3 up(0, 1, 0);
			glm::vec3 right = glm::normalize(glm::cross(dir, up));
			if (glm::length(right) < 0.001f) {
				right = glm::vec3(1, 0, 0);
			}
			glm::vec3 actual_up = glm::cross(right, dir);

			glm::vec3 spiral_offset = (right * std::sin(spiral_phase_) + actual_up * std::cos(spiral_phase_)) *
				spiral_radius_;

			// If repositioning, reduce spiral offset to move more directly
			if (repositioning_)
				spiral_offset *= 0.2f;

			glm::vec3 desired_pos = target_pos + spiral_offset;
			glm::vec3 move_dir = glm::normalize(desired_pos - current_pos);

			glm::vec3 new_vel = move_dir * current_speed;
			SetVelocity(Vector3(new_vel.x, new_vel.y, new_vel.z));
		}

		// Terrain avoidance/snap
		auto [h, norm] = handler.GetTerrainPropertiesAtPoint(current_pos.x, current_pos.z);
		if (current_pos.y < h + 2.0f) {
			SetPosition(current_pos.x, h + 2.0f, current_pos.z);
			rigid_body_.AddForce(glm::vec3(0, 150.0f, 0));
			glm::vec3 vel = GetVelocity().Toglm();
			if (vel.y < 0) {
				vel.y = -vel.y * 0.5f;
				SetVelocity(Vector3(vel.x, vel.y, vel.z));
			}
		} else if (current_pos.y < h + 10.0f) {
			rigid_body_.AddForce(glm::vec3(0, 50.0f, 0));
		}

		// Collision with player
		float dist_to_player = glm::distance(current_pos, player_pos);
		if (dist_to_player < 6.0f) {
			plane->OnHit(handler, 15.0f);
			OnHit(handler, 100.0f); // Self-destruct
		}
	}

	void CongaMarcher::OnHit(const EntityHandler& handler, float damage) {
		health_ -= damage;
		if (health_ <= 0) {
			auto pos = GetPosition().Toglm();
			handler.EnqueueVisualizerAction([pos, &handler, this]() {
				handler.vis->CreateExplosion(pos, 1.0f);
				handler.QueueRemoveEntity(this->id_);
			});
			if (auto* pp_handler = dynamic_cast<const PaperPlaneHandler*>(&handler)) {
				pp_handler->AddScore(250, "Conga Marcher Destroyed");
			}
		}
	}

} // namespace Boidsish
