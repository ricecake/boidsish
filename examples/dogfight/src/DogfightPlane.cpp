#include "DogfightPlane.h"

#include "fire_effect.h"
#include "graphics.h"
#include "spatial_entity_handler.h"
#include "terrain_generator.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

	// Helper from GuidedMissile.cpp
	static glm::vec3 CalculateSteeringTorque(
		const glm::vec3& current_forward,
		const glm::vec3& desired_direction,
		const glm::vec3& current_angular_velocity,
		float            kP,
		float            kD
	) {
		glm::vec3 error_vector = glm::cross(current_forward, desired_direction);
		glm::vec3 derivative_term = current_angular_velocity;
		return (error_vector * kP) - (derivative_term * kD);
	}

	DogfightPlane::DogfightPlane(int id, Team team, Vector3 pos): Entity<Model>(id, "assets/dogplane.obj", true), team_(team), eng_(rd_()) {
		SetPosition(pos);
		if (team == Team::RED) {
			SetColor(1.0f, 0.1f, 0.1f, 1.0f);
		} else {
			SetColor(0.1f, 0.1f, 1.0f, 1.0f);
		}
		SetSize(35.0f);
		SetTrailLength(200);
		SetTrailPBR(true);
		SetTrailRoughness(0.2f);
		SetTrailMetallic(0.8f);

		rigid_body_.linear_friction_ = 1.0f;
		rigid_body_.angular_friction_ = 5.0f;

		// Random initial orientation
		std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
		glm::vec3                             axis(dist(eng_), dist(eng_), dist(eng_));
		if (glm::length(axis) < 0.1f)
			axis = glm::vec3(0, 1, 0);
		rigid_body_.SetOrientation(glm::angleAxis(dist(eng_) * 3.14f, glm::normalize(axis)));
		rigid_body_.SetLinearVelocity(ObjectToWorld(glm::vec3(0, 0, -kSlowSpeed)));

		shape_->SetScale(glm::vec3(5.0f));
		std::dynamic_pointer_cast<Model>(shape_)->SetBaseRotation(
			glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f))
		);
		shape_->SetInstanced(true);

		UpdateShape();
	}

	void DogfightPlane::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		(void)time;
		lived_ += delta_time;

		if (exploded_) {
			if (lived_ > 2.0f) {
				handler.QueueRemoveEntity(id_);
			}
			return;
		}

		auto pos = GetPosition();
		auto [terrain_h, terrain_norm] = handler.GetTerrainPointPropertiesThreadSafe(pos.x, pos.z);

		// Collision check
		if (pos.y <= terrain_h) {
			Explode(handler);
			return;
		}

		auto&     spatial_handler = static_cast<const SpatialEntityHandler&>(handler);
		auto      nearby = spatial_handler.GetEntitiesInRadius<DogfightPlane>(pos, kDetectionRadius);
		glm::vec3 my_fwd = ObjectToWorld(glm::vec3(0, 0, -1));

		target_ = nullptr;
		chaser_ = nullptr;
		bool is_being_chased = false;

		// 1. Analyze situation
		for (auto& other : nearby) {
			if (other->GetId() == id_)
				continue;

			glm::vec3 to_other = other->GetPosition().Toglm() - pos.Toglm();
			float     dist = glm::length(to_other);
			if (dist < 1e-6f)
				continue;
			glm::vec3 dir_to_other = to_other / dist;
			glm::vec3 other_fwd = other->ObjectToWorld(glm::vec3(0, 0, -1));

			if (other->GetTeam() != team_) {
				// Potential target or chaser
				// Chaser check: they are behind us and looking at us
				float dot_me = glm::dot(my_fwd, dir_to_other);
				float dot_other = glm::dot(other_fwd, -dir_to_other);

				if (dot_me < -0.5f && dot_other > 0.7f) {
					is_being_chased = true;
					chaser_ = other;
				}

				// Basic target selection (closest enemy)
				if (!target_ || dist < pos.DistanceTo(target_->GetPosition())) {
					target_ = other;
				}
			}
		}

		// 2. Optimized Counter-chase: If an enemy is chasing an ally, prioritize that enemy
		for (auto& other : nearby) {
			if (other->GetTeam() == team_)
				continue;
			// 'other' is an enemy. Who are they chasing?
			auto enemy_target = other->GetTarget();
			if (enemy_target && enemy_target->GetTeam() == team_) {
				// This enemy is chasing one of our allies!
				target_ = other;
				break;
			}
		}

		// 3. Determine desired direction and speed
		glm::vec3 desired_dir_world = my_fwd;
		float     target_speed = kSlowSpeed;

		if (is_being_chased) {
			being_chased_timer_ += delta_time;
			target_speed = kFastSpeed;
			maneuver_time_ += delta_time;
			// Evasive: mix of away from chaser and random maneuvers
			glm::vec3 away = glm::normalize(pos.Toglm() - chaser_->GetPosition().Toglm());
			desired_dir_world = away;

			// Add loop/roll/bank
			float loop = sin(maneuver_time_ * 3.0f) * 100.0f;
			float roll = cos(maneuver_time_ * 4.0f) * 150.0f;
			float bank = sin(maneuver_time_ * 2.0f) * 80.0f;
			rigid_body_.AddRelativeTorque(glm::vec3(loop, bank, roll));

			if (being_chased_timer_ > kBeingChasedThreshold) {
				Explode(handler);
				return;
			}
		} else {
			being_chased_timer_ = std::max(0.0f, being_chased_timer_ - delta_time);
		}

		if (target_) {
			target_speed = kFastSpeed;
			glm::vec3 target_pos = target_->GetPosition().Toglm();
			glm::vec3 target_fwd = target_->ObjectToWorld(glm::vec3(0, 0, -1));
			glm::vec3 chase_pos = target_pos - target_fwd * kChaseDistance;

			glm::vec3 to_chase_pos = chase_pos - pos.Toglm();
			float     dist_to_chase = glm::length(to_chase_pos);

			if (dist_to_chase > 5.0f) {
				desired_dir_world = to_chase_pos / dist_to_chase;
			} else {
				desired_dir_world = target_fwd;
			}

			// Kill check
			glm::vec3 to_target = target_pos - pos.Toglm();
			float     dist_to_target = glm::length(to_target);
			glm::vec3 dir_to_target = to_target / dist_to_target;
			float     aim_dot = glm::dot(my_fwd, dir_to_target);
			float     victim_dot = glm::dot(target_fwd, my_fwd); // should be positive if we are behind them

			if (dist_to_target < kKillDistance && aim_dot > kKillAngle && victim_dot > kKillBehindAngle) {
				fire_timer_ += delta_time;
				if (fire_timer_ > kKillTimeThreshold) {
					target_->Explode(handler);
					fire_timer_ = 0.0f;
				}
			} else {
				fire_timer_ = std::max(0.0f, fire_timer_ - delta_time);
			}
		} else if (!is_being_chased) {
			// Flocking
			Vector3 cohesion(0, 0, 0);
			Vector3 separation(0, 0, 0);
			Vector3 alignment(0, 0, 0);
			int     allies = 0;

			for (auto& other : nearby) {
				if (other->GetId() == id_ || other->GetTeam() != team_)
					continue;
				cohesion += other->GetPosition();
				alignment += other->GetVelocity();
				float dist = pos.DistanceTo(other->GetPosition());
				if (dist < 20.0f && dist > 0.01f) {
					separation += (pos - other->GetPosition()) / (dist * dist);
				}
				allies++;
			}

			if (allies > 0) {
				cohesion = (cohesion / (float)allies) - pos;
				alignment = alignment / (float)allies;
				desired_dir_world = glm::normalize(
					my_fwd + cohesion.Toglm() * 0.05f + separation.Toglm() * 2.0f + alignment.Toglm() * 0.1f
				);
			}

			// Circle if nothing else
			glm::vec3 center(0, 100, 0);
			glm::vec3 to_center = center - pos.Toglm();
			glm::vec3 orbit = glm::cross(glm::vec3(0, 1, 0), glm::normalize(to_center));
			desired_dir_world = glm::normalize(desired_dir_world + orbit * 0.5f + to_center * 0.01f);
		}

		// Terrain Hugging
		float target_h = terrain_h + 30.0f;
		float h_err = target_h - pos.y;
		desired_dir_world.y += h_err * 0.05f;
		desired_dir_world = glm::normalize(desired_dir_world);

		// Terrain Avoidance (Raycast)
		const auto* terrain_gen = handler.GetTerrainGenerator();
		if (terrain_gen) {
			float     hit_dist = 0.0f;
			glm::vec3 ray_dir = my_fwd;
			if (terrain_gen->Raycast(pos.Toglm(), ray_dir, 100.0f, hit_dist)) {
				auto [h, n] = terrain_gen->pointProperties(pos.x + ray_dir.x * hit_dist, pos.z + ray_dir.z * hit_dist);
				glm::vec3 away = n;
				if (glm::dot(away, glm::vec3(0, 1, 0)) < 0.5f)
					away = glm::vec3(0, 1, 0);
				float weight = 1.0f - (hit_dist / 100.0f);
				desired_dir_world = glm::normalize(desired_dir_world + away * weight * 5.0f);
			}
		}

		// Apply Steering
		glm::vec3 local_fwd(0, 0, -1);
		glm::vec3 desired_dir_local = WorldToObject(desired_dir_world);
		glm::vec3 torque =
			CalculateSteeringTorque(local_fwd, desired_dir_local, rigid_body_.GetAngularVelocity(), 50.0f, 6.0f);
		rigid_body_.AddRelativeTorque(torque);

		// Speed control
		rigid_body_.AddRelativeForce(glm::vec3(0, 0, -500.0f)); // constant thrust
		glm::vec3 vel = rigid_body_.GetLinearVelocity();
		float     speed = glm::length(vel);
		if (speed > target_speed) {
			rigid_body_.SetLinearVelocity(vel * (target_speed / speed));
		} else if (speed < kSlowSpeed * 0.5f) {
			rigid_body_.SetLinearVelocity(glm::normalize(vel) * kSlowSpeed * 0.5f);
		}
	}

	void DogfightPlane::Explode(const EntityHandler& handler) {
		if (exploded_)
			return;
		exploded_ = true;
		lived_ = 0.0f;
		SetVelocity(0, 0, 0);
		SetSize(0);

		auto pos = GetPosition().Toglm();
		handler.EnqueueVisualizerAction([=, &handler]() {
			handler.vis
				->AddFireEffect(pos, FireEffectStyle::Explosion, glm::vec3(0, 1, 0), glm::vec3(0, 0, 0), -1, 2.0f);
			handler.vis->AddSoundEffect("assets/rocket_explosion.wav", pos, glm::vec3(0), 20.0f);
		});
	}

} // namespace Boidsish
