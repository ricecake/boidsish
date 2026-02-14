#include "PearEnemy.h"

#include "logger.h"
#include "PaperPlane.h"
#include "PaperPlaneHandler.h"
#include "MagentaBall.h"
#include "spatial_entity_handler.h"
#include "terrain_generator_interface.h"
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

	PearEnemy::PearEnemy(int id, Vector3 pos): Entity<Model>(id, "assets/utah_teapot.obj"), eng_(rd_()) {
		SetPosition(pos);
		SetColor(0.82f, 0.71f, 0.55f, 1.0f); // Tan
		SetTrailLength(0);                  // Remove grey lines
		shape_->SetScale(glm::vec3(2.0f));
		shape_->SetInstanced(true);

		rigid_body_.linear_friction_ = 1.0f;
		rigid_body_.angular_friction_ = 2.0f;
		rigid_body_.SetMaxLinearVelocity(15.0f);

		probe_.SetPosition(pos.Toglm());
		probe_.SetFlyHeight(1.5f);
		probe_.SetValleySlideStrength(50.0f);
		probe_.SetSpringStiffness(8.0f);

		UpdateShape();
	}

	// void PearEnemy::UpdateShape() {
	// 	Entity<Model>::UpdateShape();
	// 	shape_->SetScale(glm::vec3(2.0f));
	// }

	void PearEnemy::OnHit(float damage) {
		health_ -= damage;
	}

	void PearEnemy::Destroy(const EntityHandler& handler) {
		// Award points
		if (auto* pp_handler = dynamic_cast<const PaperPlaneHandler*>(&handler)) {
			pp_handler->AddScore(250, "Ground Unit Destroyed");
		}

		auto pos = GetPosition().Toglm();
		auto [h_val, n_val] = handler.GetTerrainPropertiesAtPoint(pos.x, pos.z);
		float     height = h_val;
		glm::vec3 normal = n_val;
		auto      vis = handler.vis;
		auto      shape = this->shape_;
		int       my_id = GetId();

		handler.EnqueueVisualizerAction([vis, shape, normal, pos, height]() {
			if (vis) {
				vis->TriggerComplexExplosion(shape, normal, 1.5f, FireEffectStyle::Explosion);
				if (auto terrain = vis->GetTerrain()) {
					terrain->AddCrater({pos.x, height, pos.z}, 10.0f, 5.0f, 0.2f, 1.5f);
				}
				vis->AddSoundEffect("assets/rocket_explosion.wav", pos, glm::vec3(0), 15.0f);
			}
		});
		handler.QueueRemoveEntity(my_id);
	}

	void PearEnemy::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		if (health_ <= 0.0f) {
			Destroy(handler);
			return;
		}

		// Update probe terrain if not already set
		if (handler.vis && handler.vis->GetTerrain()) {
			probe_.SetTerrain(handler.vis->GetTerrain());
		}

		Roam(handler, delta_time);

		if (attack_cooldown_ > 0.0f) {
			attack_cooldown_ -= delta_time;
		}

		// Player detection and Probe Update
		auto planes = handler.GetEntitiesByType<PaperPlane>();
		if (!planes.empty()) {
			PaperPlane* plane = planes[0];
			glm::vec3   plane_pos = plane->GetPosition().Toglm();
			glm::vec3   plane_vel = plane->GetVelocity().Toglm();

			// Update probe towards player if nearby, otherwise it roams in Roam()
			float dist_to_plane = glm::distance(GetPosition().Toglm(), plane_pos);
			if (dist_to_plane < detection_radius_) {
				probe_.Update(delta_time, plane_pos, plane_vel);
			}

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
		glm::vec3 current_pos = GetPosition().Toglm();
		glm::vec3 vel = rigid_body_.GetLinearVelocity();

		if (wait_timer_ > 0.0f) {
			wait_timer_ -= delta_time;
		}

		// 1. Target selection and Probe Update (if not following player)
		bool following_player = false;
		auto planes = handler.GetEntitiesByType<PaperPlane>();
		if (!planes.empty()) {
			if (glm::distance(current_pos, planes[0]->GetPosition().Toglm()) < detection_radius_) {
				following_player = true;
			}
		}

		if (!following_player) {
			if (!has_target_ && wait_timer_ <= 0.0f) {
				std::uniform_real_distribution<float> dist(-150.0f, 150.0f);
				glm::vec3                             candidate = current_pos + glm::vec3(dist(eng_), 0.0f, dist(eng_));

				auto [h, norm] = handler.vis->GetTerrainPropertiesAtPoint(candidate.x, candidate.z);

				if (h < 40.0f && norm.y > 0.7f) {
					target_pos_ = glm::vec3(candidate.x, h, candidate.z);
					has_target_ = true;
				}
			}

			if (has_target_) {
				probe_.Update(delta_time, target_pos_, glm::vec3(0.0f));
			}
		}

		// 2. Horizontal Movement Force (Move towards Probe)
		glm::vec3 move_force(0.0f);
		glm::vec3 move_dir(0.0f);

		glm::vec3 to_probe = probe_.GetPosition() - current_pos;
		to_probe.y = 0;
		float dist_to_probe = glm::length(to_probe);

		if (dist_to_probe > 0.1f) {
			move_dir = to_probe / dist_to_probe;
			float speed_factor = std::clamp(dist_to_probe * 0.5f, 0.0f, 1.0f);
			move_force = move_dir * 25.0f * speed_factor;
		}

		if (has_target_ && !following_player) {
			glm::vec3 to_target = target_pos_ - current_pos;
			to_target.y = 0;
			float dist_to_target = glm::length(to_target);
			if (dist_to_target < 8.0f) {
				has_target_ = false;
				wait_timer_ = 3.0f;
			}
		}

		// 3. Separation Force
		glm::vec3 separation_force(0.0f);
		if (auto spatial_handler = dynamic_cast<const SpatialEntityHandler*>(&handler)) {
			const auto& neighbors = spatial_handler->GetEntitiesInRadius<PearEnemy>(GetPosition(), 30.0f);
			for (const auto& neighbor : neighbors) {
				if (neighbor->GetId() == GetId()) continue;
				glm::vec3 diff = current_pos - neighbor->GetPosition().Toglm();
				float     dist = glm::length(diff);
				if (dist > 0.001f) {
					separation_force += (diff / (dist * dist)) * 100.0f; // Increased from 20.0f
				}
			}
		}

		// 4. Terrain Avoidance (don't walk into walls)
		glm::vec3 avoidance_force(0.0f);
		if (glm::length(vel) > 0.1f) {
			glm::vec3 look_ahead_pos = current_pos + glm::normalize(vel) * 10.0f;
			auto [h_ahead, norm_ahead] = handler.vis->GetTerrainPropertiesAtPoint(look_ahead_pos.x, look_ahead_pos.z);
			if (h_ahead > current_pos.y + 2.0f) {
				avoidance_force = -glm::normalize(vel) * 30.0f;
			}
		}

		rigid_body_.AddForce(move_force + separation_force + avoidance_force);

		// 5. Vertical Stability (PD controller)
		auto [h, norm] = handler.vis->GetTerrainPropertiesAtPoint(current_pos.x, current_pos.z);
		float target_h = h + 1.5f;
		float error = target_h - current_pos.y;
		float force_y = error * 60.0f - vel.y * 25.0f;
		force_y = glm::clamp(force_y, -150.0f, 150.0f);
		rigid_body_.AddForce(glm::vec3(0, force_y, 0));

		// 6. Orientation (Align with normal and movement)
		glm::vec3 up = (glm::length(norm) > 0.01f) ? glm::normalize(norm) : glm::vec3(0, 1, 0);
		glm::vec3 forward_pref = (glm::length(vel) > 0.5f) ? glm::normalize(vel) : (glm::length(move_dir) > 0.01f ? move_dir : rigid_body_.GetOrientation() * glm::vec3(0, 0, -1));

		glm::vec3 forward = forward_pref - up * glm::dot(forward_pref, up);
		if (glm::length(forward) < 0.001f) forward = rigid_body_.GetOrientation() * glm::vec3(0, 0, -1);
		forward = glm::normalize(forward);

		glm::vec3 right = glm::normalize(glm::cross(up, forward));
		forward = glm::cross(right, up);

		glm::mat4 rotation_matrix = glm::mat4(1.0f);
		rotation_matrix[0] = glm::vec4(right, 0.0f);
		rotation_matrix[1] = glm::vec4(up, 0.0f);
		rotation_matrix[2] = glm::vec4(-forward, 0.0f);

		glm::quat target_orient = glm::quat_cast(rotation_matrix);
		glm::quat current_orient = rigid_body_.GetOrientation();
		SetOrientation(glm::slerp(current_orient, target_orient, 5.0f * delta_time));
	}

} // namespace Boidsish
