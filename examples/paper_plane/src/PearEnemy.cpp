#include "PearEnemy.h"

#include "logger.h"
#include "PaperPlane.h"
#include "PaperPlaneHandler.h"
#include "MagentaBall.h"
#include "terrain_generator_interface.h"
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

	PearEnemy::PearEnemy(int id, Vector3 pos): Entity<PearShape>(id), eng_(rd_()) {
		SetPosition(pos);
		shape_->SetScale(glm::vec3(2.0f)); // Make them reasonably sized
		shape_->SetInstanced(true);
		UpdateShape();
	}

	void PearEnemy::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		Roam(handler, delta_time);

		if (attack_cooldown_ > 0.0f) {
			attack_cooldown_ -= delta_time;
		}

		// Player detection
		auto planes = handler.GetEntitiesByType<PaperPlane>();
		if (!planes.empty()) {
			PaperPlane* plane = planes[0];
			glm::vec3   plane_pos = plane->GetPosition().Toglm();
			glm::vec3   my_pos = GetPosition().Toglm() + glm::vec3(0, 1.5f, 0); // Eye level
			float       dist = glm::distance(my_pos, plane_pos);

			if (dist < detection_radius_) {
				// Line of sight check
				glm::vec3 dir = glm::normalize(plane_pos - my_pos);
				float     hit_dist = 0.0f;
				glm::vec3 hit_norm;
				bool      hit = handler.RaycastTerrain(my_pos, dir, dist, hit_dist, hit_norm);

				if (!hit || hit_dist >= dist - 1.0f) {
					// Player is visible!
					if (attack_cooldown_ <= 0.0f) {
						// Launch magenta ball in a ballistic arc
						glm::vec3 to_player = plane_pos - my_pos;
						float horiz_dist = glm::length(glm::vec3(to_player.x, 0, to_player.z));
						float vert_dist = to_player.y;

						// Simple ballistic calculation for long arc
						// v_x = horiz_dist / time
						// v_y = (vert_dist + 0.5 * g * time^2) / time
						float time_to_impact = 3.0f; // Target 3 seconds flight
						float g = 9.8f;

						glm::vec3 vel;
						vel.x = to_player.x / time_to_impact;
						vel.z = to_player.z / time_to_impact;
						vel.y = (vert_dist + 0.5f * g * time_to_impact * time_to_impact) / time_to_impact;

						handler.QueueAddEntity<MagentaBall>(Vector3(my_pos.x, my_pos.y, my_pos.z), Vector3(vel.x, vel.y, vel.z));
						attack_cooldown_ = 5.0f; // 5 seconds between shots
					}
				}
			}

			// Despawn logic (part of next step but I can prepare it)
			if (dist > 1500.0f) {
				handler.QueueRemoveEntity(GetId());
			}
		}

		UpdateShape();
	}

	void PearEnemy::Roam(const EntityHandler& handler, float delta_time) {
		if (wait_timer_ > 0.0f) {
			wait_timer_ -= delta_time;
			return;
		}

		if (!has_target_) {
			// Pick a new target position
			std::uniform_real_distribution<float> dist(-50.0f, 50.0f);
			glm::vec3 current_pos = GetPosition().Toglm();
			glm::vec3 candidate = current_pos + glm::vec3(dist(eng_), 0.0f, dist(eng_));

			auto [h, norm] = handler.GetCachedTerrainProperties(candidate.x, candidate.z);

			// Constraint: height < 50, and prefer flat ground (norm.y is large)
			if (h < 50.0f && norm.y > 0.8f) {
				target_pos_ = glm::vec3(candidate.x, h, candidate.z);
				has_target_ = true;
			}
			return;
		}

		glm::vec3 current_pos = GetPosition().Toglm();
		glm::vec3 to_target = target_pos_ - current_pos;
		float     dist_to_target = glm::length(to_target);

		if (dist_to_target < 1.0f) {
			has_target_ = false;
			std::uniform_real_distribution<float> wait_dist(1.0f, 3.0f);
			wait_timer_ = wait_dist(eng_);
			return;
		}

		glm::vec3 move_dir = to_target / dist_to_target;
		glm::vec3 new_pos = current_pos + move_dir * move_speed_ * delta_time;

		// Keep on terrain
		auto [h, norm] = handler.GetCachedTerrainProperties(new_pos.x, new_pos.z);
		new_pos.y = h;

		SetPosition(new_pos.x, new_pos.y, new_pos.z);

		// Align with terrain normal and face movement direction
		glm::vec3 up = norm;
		glm::vec3 forward = move_dir;
		if (glm::length(forward) < 0.001f) forward = glm::vec3(0,0,1);
		glm::vec3 right = glm::normalize(glm::cross(up, forward));
		forward = glm::cross(right, up);

		glm::mat4 rotation_matrix = glm::mat4(1.0f);
		rotation_matrix[0] = glm::vec4(right, 0.0f);
		rotation_matrix[1] = glm::vec4(up, 0.0f);
		rotation_matrix[2] = glm::vec4(-forward, 0.0f);

		SetOrientation(glm::quat_cast(rotation_matrix));
	}

} // namespace Boidsish
