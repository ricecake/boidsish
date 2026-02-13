#include "PearEnemy.h"

#include "logger.h"
#include "PaperPlane.h"
#include "PaperPlaneHandler.h"
#include "MagentaBall.h"
#include "terrain_generator_interface.h"
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

	PearEnemy::PearEnemy(int id, Vector3 pos): Entity<Model>(id, "assets/utah_teapot.obj"), eng_(rd_()) {
		SetPosition(pos);
		SetColor(0.82f, 0.71f, 0.55f, 1.0f); // Tan
		SetTrailLength(0);                  // Remove grey lines
		shape_->SetScale(glm::vec3(1.0f));   // Larger teapot
		shape_->SetInstanced(true);

		rigid_body_.linear_friction_ = 1.0f;
		rigid_body_.angular_friction_ = 2.0f;

		UpdateShape();
	}

	void PearEnemy::UpdateShape() {
		Entity<Model>::UpdateShape();
		shape_->SetScale(glm::vec3(2.0f));
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
						// Intercept prediction: target where player will be in 2 seconds
						float     time_to_impact = 2.0f;
						glm::vec3 player_vel = plane->GetVelocity().Toglm();
						glm::vec3 target_pos = plane_pos + player_vel * time_to_impact;

						glm::vec3 to_target = target_pos - my_pos;

						// Ballistic calculation
						float g = 9.8f;
						glm::vec3 vel;
						vel.x = to_target.x / time_to_impact;
						vel.z = to_target.z / time_to_impact;
						vel.y = (to_target.y + 0.5f * g * time_to_impact * time_to_impact) / time_to_impact;

						// Add an ID argument for MagentaBall
						int projectile_id = 0x70000000 | (GetId() & 0x0FFFFFFF);
						handler.QueueAddEntity<MagentaBall>(projectile_id, Vector3{my_pos.x, my_pos.y, my_pos.z}, Vector3{vel.x, vel.y, vel.z});
						attack_cooldown_ = 5.0f; // 5 seconds between shots
					}
				}
			}
		}

		UpdateShape();
	}

	void PearEnemy::Roam(const EntityHandler& handler, float delta_time) {
		if (wait_timer_ > 0.0f) {
			wait_timer_ -= delta_time;
			return;
		}

		glm::vec3 current_pos = GetPosition().Toglm();
		glm::vec3 vel = rigid_body_.GetLinearVelocity();

		if (!has_target_) {
			// Pick a new target position
			std::uniform_real_distribution<float> dist(-100.0f, 100.0f);
			glm::vec3                             candidate = current_pos + glm::vec3(dist(eng_), 0.0f, dist(eng_));

			auto [h, norm] = handler.vis->GetTerrain()->GetPointProperties(candidate.x, candidate.z);

			// Constraint: height < 50, and prefer flat ground (norm.y is large)
			if (h < 50.0f && norm.y > 0.8f) {
				target_pos_ = glm::vec3(candidate.x, h, candidate.z);
				has_target_ = true;
			}
			return;
		}

		glm::vec3 to_target = target_pos_ - current_pos;
		to_target.y = 0; // Horizontal distance
		float dist_to_target = glm::length(to_target);

		if (dist_to_target < 5.0f) {
			has_target_ = false;
			std::uniform_real_distribution<float> wait_dist(1.0f, 3.0f);
			wait_timer_ = wait_dist(eng_);
			return;
		}

		glm::vec3 move_dir = (dist_to_target > 0.01f) ? to_target / dist_to_target : glm::vec3(0);
		rigid_body_.AddForce(move_dir * 30.0f); // Constant horizontal movement force

		// Keep on terrain using vertical spring-damping
		auto [h, norm] = handler.vis->GetTerrain()->GetPointProperties(current_pos.x, current_pos.z);
		float target_h = h + 1.5f; // Target height above ground (teapot center)
		float error = target_h - current_pos.y;

		// Simple PD controller for vertical stability
		float force_y = error * 100.0f - vel.y * 20.0f;
		force_y = glm::clamp(force_y, -500.0f, 500.0f); // Prevent explosive forces
		rigid_body_.AddForce(glm::vec3(0, force_y, 0));

		// Align with terrain normal and face movement direction
		glm::vec3 up = (glm::length(norm) > 0.01f) ? glm::normalize(norm) : glm::vec3(0, 1, 0);
		glm::vec3 forward_pref = (glm::length(vel) > 0.1f) ? glm::normalize(vel) : move_dir;
		if (glm::length(forward_pref) < 0.001f) forward_pref = glm::vec3(0, 0, 1);

		// Project forward onto terrain tangent plane
		glm::vec3 forward = forward_pref - up * glm::dot(forward_pref, up);
		if (glm::length(forward) < 0.001f) {
			forward = glm::vec3(1, 0, 0) - up * up.x; // Fallback
		}
		forward = glm::normalize(forward);

		glm::vec3 right = glm::normalize(glm::cross(up, forward));
		forward = glm::cross(right, up); // Re-orthogonalize

		glm::mat4 rotation_matrix = glm::mat4(1.0f);
		rotation_matrix[0] = glm::vec4(right, 0.0f);
		rotation_matrix[1] = glm::vec4(up, 0.0f);
		rotation_matrix[2] = glm::vec4(-forward, 0.0f);

		glm::quat target_orient = glm::quat_cast(rotation_matrix);
		glm::quat current_orient = rigid_body_.GetOrientation();
		SetOrientation(glm::slerp(current_orient, target_orient, 10.0f * delta_time));
	}

} // namespace Boidsish
