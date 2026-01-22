#include "CatMissile.h"

#include "PaperPlane.h"
#include "fire_effect.h"
#include "graphics.h"
#include "spatial_entity_handler.h"
#include "terrain_generator.h"
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

// Calculates the torque needed to rotate 'current_forward' to align with 'desired_direction'
// using a PD controller to prevent overshoot.
glm::vec3 CalculateSteeringTorque(
    const glm::vec3& current_forward,
    const glm::vec3& desired_direction,
    const glm::vec3& current_angular_velocity,
    float kP, // Proportional Gain (Strength of the turn)
    float kD  // Derivative Gain (Damping/Stability)
) {
    // 1. Calculate the Error (The "P" term)
    // The cross product gives us the axis of rotation perpendicular to both vectors.
    // The length of this vector is proportional to the sin(angle) between them.
    glm::vec3 error_vector = glm::cross(current_forward, desired_direction);

    // 2. Calculate the "Braking" (The "D" term)
    // We want to oppose the current rotation speed.
    // We only care about the angular velocity relative to our turning plane,
    // but typically using the whole vector works fine for missiles.
    glm::vec3 derivative_term = current_angular_velocity;

    // 3. Combine them
    // Torque = (Strength * Error) - (Damping * Velocity)
    return (error_vector * kP) - (derivative_term * kD);
}

	CatMissile::CatMissile(int id, Vector3 pos, glm::quat orientation, glm::vec3 dir, Vector3 vel):
		Entity<Model>(id, "assets/Missile.obj", true), eng_(rd_()) {
		rigid_body_.linear_friction_ = 0.0f;
		rigid_body_.angular_friction_ = 0.0f;

		rigid_body_.SetOrientation(orientation);
		SetPosition(pos.x, pos.y, pos.z);
		rigid_body_.SetLinearVelocity(glm::vec3(vel.x, vel.y, vel.z) + 5.0f * dir);

		SetTrailLength(0);
		SetTrailRocket(false);
		shape_->SetScale(glm::vec3(0.05f));
		std::dynamic_pointer_cast<Model>(shape_)->SetBaseRotation(
			glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f))
		);
	}

	void CatMissile::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		lived_ += delta_time;
		auto pos = GetPosition();

		if (exploded_) {
			if (lived_ >= kExplosionDisplayTime) {
				handler.QueueRemoveEntity(id_);
			}
			return;
		}

		if (lived_ >= lifetime_) {
			Explode(handler, false);
			return;
		}

		auto [height, norm] = handler.vis->GetTerrainPointPropertiesThreadSafe(pos.x, pos.z);
		if (pos.y <= height) {
			Explode(handler, false);
			return;
		}

		const float kLaunchTime = 1.0f;
		const float kMaxSpeed = 150.0f;
		const float kAcceleration = 150.0f;

		if (lived_ < kLaunchTime) {
			// auto velo = GetVelocity();
			// velo += Vector3(0, -0.07f, 0);
			// SetVelocity(velo);
			rigid_body_.AddForce(glm::vec3(0, -1.0, 0));
			return;
		}

		if (!fired_) {
			SetTrailLength(500);
			SetTrailRocket(true);
			// SetOrientToVelocity(true);
			launch_sound_ = handler.vis->AddSoundEffect("assets/rocket.wav", pos.Toglm(), GetVelocity().Toglm(), 10.0f);

			rigid_body_.linear_friction_ = 7.5f;
			rigid_body_.angular_friction_ = 7.5f;

			fired_ = true;
		} else if (launch_sound_) {
			launch_sound_->SetPosition(pos.Toglm());
		}

		rigid_body_.AddRelativeForce(glm::vec3(0, 0, -2000));

		if (lived_ <= kLaunchTime + 0.5f) {
			return;
		}

		// forward_speed_ += kAcceleration * delta_time;
		// if (forward_speed_ > kMaxSpeed) {
		// 	forward_speed_ = kMaxSpeed;
		// }


		const float kTurnSpeed = 100.0f;
		const float kDamping = 2.5f;

			auto& spatial_handler = static_cast<const SpatialEntityHandler&>(handler);
			auto  targets = spatial_handler.GetEntitiesInRadius<GuidedMissileLauncher>(
                pos,
                kMaxSpeed * (lifetime_ - lived_) * 0.5f
            );

			const float stickiness = 0.65f;
			auto        minRank = INFINITY;
			for (auto& candidate : targets) {
				auto target_pos = candidate->GetPosition().Toglm();
				auto missile_pos = pos.Toglm();

				auto world_fwd = rigid_body_.GetOrientation() * glm::vec3(0, 0, -1);
				auto to_target = normalize(target_pos - missile_pos);
				auto distance = glm::length(missile_pos - target_pos);

				auto frontNess = glm::dot(world_fwd, to_target);
				if (frontNess < 0.85) {
					continue;
				}

				// auto rank = distance * (4.0 - 3.5 * frontNess);
				auto rank = distance * (2.0 - 1.75f * frontNess);
				if (candidate == target_) {
					rank *= stickiness;
				}
				// logger::LOG("Checking", candidate->GetId(), rank, frontNess, distance);

				if (rank < minRank) {
					minRank = rank;
					target_ = candidate;
					// logger::LOG("Seeking", candidate->GetId(), rank, frontNess, distance);
				}
			}

		glm::vec3 target_dir_world;
		glm::vec3 target_dir_local = glm::vec3(0,0,-1);
		if (target_ != nullptr) {
			if ((target_->GetPosition() - GetPosition()).Magnitude() < 10) {
				Explode(handler, true);
				return;
			}

		// logger::LOG("Seeking", target_->GetId(), glm::length(pos.Toglm() - target_->GetPosition().Toglm()));

			Vector3   target_vec = (target_->GetPosition() - GetPosition()).Normalized();
			target_dir_world = glm::vec3(target_vec.x, target_vec.y, target_vec.z);

			target_dir_local = WorldToObject(target_dir_world);
		}

		const auto* terrain_generator = handler.GetTerrainGenerator();
		if (terrain_generator) {
			const float reaction_distance = 100.0f;
			const float kAvoidanceStrength = 5.0f;
			const float kUpAlignmentThreshold = 0.5f;

			Vector3 vel_vec = GetVelocity();
			if (vel_vec.MagnitudeSquared() > 1e-6) {
				glm::vec3 origin = {GetPosition().x, GetPosition().y, GetPosition().z};
				glm::vec3 dir = {vel_vec.x, vel_vec.y, vel_vec.z};
				dir = glm::normalize(dir);

				float       hit_dist = 0.0f;
				if (terrain_generator->Raycast(origin, dir, reaction_distance, hit_dist)) {
					auto hit_coord = vel_vec.Normalized() * hit_dist;
					auto [terrain_h, terrain_normal] = terrain_generator->pointProperties(hit_coord.x, hit_coord.z);


					glm::vec3 local_up = glm::vec3(0.0f, 1.0f, 0.0f);
					auto      away = terrain_normal;
					if (glm::dot(away, local_up) < kUpAlignmentThreshold) {
						away = local_up;
					}

					if(target_ != nullptr) {
						away = target_dir_world - (glm::dot(target_dir_world, away)) * away;
					}

					float distance_factor = 1.0f - (hit_dist / reaction_distance);
					float alignment_with_target = glm::dot(dir, target_dir_world);
					float target_priority = 1.0f - glm::clamp(alignment_with_target, 0.0f, 1.0f);
					float avoidance_weight = distance_factor * target_priority;
					glm::vec3 final_desired_dir = glm::normalize(target_dir_world + (away * avoidance_weight * kAvoidanceStrength));
					target_dir_local = WorldToObject(final_desired_dir);

					/*

				glm::vec3 target_dir_local = WorldToObject(away);
					glm::vec3 P = glm::vec3(0, 0, -1);
					glm::vec3 torque = glm::cross(P, target_dir_local);
					rigid_body_.AddRelativeTorque(torque * kTurnSpeed);


					away = target_dir_world - (glm::dot(target_dir_world, away)) * away;

					const float avoidance_strength = 5.0f;
					float force_magnitude = avoidance_strength * (1.0f - ((10 + hit_dist) / reaction_distance));
					glm::vec3 avoidance_force = away * force_magnitude * (1 - glm::dot(dir, target_dir_world));
					glm::vec3 avoidance_local = glm::inverse(orientation_) * avoidance_force;
					rotational_velocity_.y += avoidance_local.x * avoidance_strength * delta_time;
					rotational_velocity_.x += avoidance_local.y * avoidance_strength * delta_time;
					*/

				}
			}



		}

			glm::vec3 local_forward = glm::vec3(0, 0, -1);
			glm::vec3 pid_torque = CalculateSteeringTorque(
					local_forward,
					target_dir_local,
					rigid_body_.GetAngularVelocity(),
					100.0f, //kP,
					20.0f//kD
				);

			rigid_body_.AddRelativeTorque(pid_torque);

	}

	void CatMissile::Explode(const EntityHandler& handler, bool hit_target) {
		if (exploded_)
			return;

		auto pos = GetPosition();
		handler.EnqueueVisualizerAction([=, &handler]() {
			handler.vis->AddFireEffect(
				glm::vec3(pos.x, pos.y, pos.z),
				FireEffectStyle::Explosion,
				glm::vec3(0, 1, 0),
				glm::vec3(0, 0, 0),
				-1,
				5.0f
			);
		});

		handler.EnqueueVisualizerAction([exhaust = exhaust_effect_]() {
			if (exhaust) {
				exhaust->SetLifetime(0.25f);
				exhaust->SetLived(0.0f);
			}
		});

		exploded_ = true;
		lived_ = 0.0f;
		SetVelocity(Vector3(0, 0, 0));
		explode_sound_ = handler.vis
							 ->AddSoundEffect("assets/rocket_explosion.wav", pos.Toglm(), GetVelocity().Toglm(), 25.0f);

		if (hit_target) {
			SetSize(100);
			SetColor(1, 0, 0, 0.33f);
		}
	}

} // namespace Boidsish
