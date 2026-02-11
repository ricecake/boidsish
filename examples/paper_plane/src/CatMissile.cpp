#include "CatMissile.h"

#include <algorithm>
#include <cmath>

#include "PaperPlane.h"
#include "PaperPlaneHandler.h"
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
		float            kP, // Proportional Gain (Strength of the turn)
		float            kD  // Derivative Gain (Damping/Stability)
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

	glm::vec3
	GetInterceptPoint(glm::vec3 shooter_pos, float shooter_speed, glm::vec3 target_pos, glm::vec3 target_vel) {
		glm::vec3 target_dir = target_pos - shooter_pos;
		float     dist = glm::length(target_dir);

		// Simple time-to-impact estimation
		// (You can make this more complex with quadratic equations, but this works for games)
		float time_to_impact = dist / shooter_speed;

		// Predict where the target will be
		return target_pos + (target_vel * time_to_impact);
	}

	CatMissile::CatMissile(int id, Vector3 pos, glm::quat orientation, glm::vec3 dir, Vector3 vel, bool leftHanded):
		Entity<Model>(id, "assets/Missile.obj", true), eng_(rd_()), leftHanded_(leftHanded) {
		rigid_body_.linear_friction_ = 0.01f;
		rigid_body_.angular_friction_ = 0.01f;

		rigid_body_.SetOrientation(orientation);
		SetPosition(pos.x, pos.y, pos.z);

		glm::vec3 world_eject = orientation * dir;
		rigid_body_.SetLinearVelocity(glm::vec3(vel.x, vel.y, vel.z) + (5.0f * world_eject));

		SetTrailLength(0);
		SetTrailRocket(false);
		shape_->SetScale(glm::vec3(0.05f));
		std::dynamic_pointer_cast<Model>(shape_)->SetBaseRotation(
			glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f))
		);
		shape_->SetInstanced(true);
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

		auto [height, norm] = handler.GetCachedTerrainProperties(pos.x, pos.z);
		if (pos.y <= height) {
			Explode(handler, false);
			return;
		}

		const float kLaunchTime = 1.0f;
		const float kMaxSpeed = 150.0f;
		const float kAcceleration = 150.0f;

		if (lived_ < kLaunchTime) {
			rigid_body_.AddForce(glm::vec3(0, -1.00, 0));
			return;
		}

		if (!fired_) {
			SetTrailLength(300);
			SetTrailRocket(true);
			handler.EnqueueVisualizerAction([this, &handler, pos]() {
				this->launch_sound_ =
					handler.vis->AddSoundEffect("assets/rocket.wav", pos.Toglm(), GetVelocity().Toglm(), 10.0f);
			});

			rigid_body_.linear_friction_ = 7.5f;
			rigid_body_.angular_friction_ = 7.5f;

			fired_ = true;
		} else if (launch_sound_) {
			handler.EnqueueVisualizerAction([effect = launch_sound_, p = pos.Toglm()]() {
				effect->SetPosition(p);
			});
		}

		rigid_body_.AddRelativeForce(glm::vec3(0, 0, -2000));

		if (lived_ <= kLaunchTime + 0.5f) {
			return;
		}

		const float kTurnSpeed = 100.0f;
		const float kDamping = 2.5f;

		auto& pp_handler = static_cast<const PaperPlaneHandler&>(handler);
		auto  targets = pp_handler.GetEntitiesInRadius<GuidedMissileLauncher>(
            pos,
            kMaxSpeed * (lifetime_ - lived_) * 0.5f
        );

		const float stickiness = 0.30f;
		auto        minRank = INFINITY;
		float       target_distance = 0.0f;
		const float reaction_distance = 250.0f;

		for (auto& candidate : targets) {
			auto target_pos = candidate->GetPosition().Toglm();
			auto missile_pos = pos.Toglm();

			auto world_fwd = rigid_body_.GetOrientation() * glm::vec3(0, 0, -1);
			auto to_target = normalize(target_pos - missile_pos);
			auto distance = glm::length(missile_pos - target_pos);

			auto frontNess = glm::dot(world_fwd, to_target);

			if (distance <= 5 && frontNess < 0.75 || distance <= 10 && frontNess < 0.85) {
				target_ = candidate;
				Explode(handler, true);
				return;
			}

			if (frontNess < 0.80) {
				continue;
			}

			float     hit_dist = 0.0f;
			glm::vec3 terrain_normal;
			bool target_blocked = handler.RaycastTerrain(missile_pos, to_target, distance, hit_dist, terrain_normal);
			bool sector_blocked = true;

			if (target_blocked) {
				glm::vec3 approach_p = candidate->GetApproachPoint();
				glm::vec3 to_approach = glm::normalize(approach_p - missile_pos);
				float     dist_to_approach = glm::length(approach_p - missile_pos);
				sector_blocked =
					handler.RaycastTerrain(missile_pos, to_approach, dist_to_approach, hit_dist, terrain_normal);
			} else {
				sector_blocked = false;
			}

			if (target_blocked && sector_blocked) {
				continue;
			}

			int  target_count = pp_handler.GetTargetCount(candidate);
			auto rank = distance * (2.0 - 1.75f * frontNess) * (1.0f + 0.5f * target_count);
			if (candidate == target_) {
				rank *= stickiness;
			}

			if (rank < minRank) {
				target_distance = distance;
				minRank = rank;
				target_ = candidate;
			}
		}

		// pp_handler.RecordTarget(target_);

		glm::vec3 world_fwd = rigid_body_.GetOrientation() * glm::vec3(0, 0, -1);
		glm::vec3 target_dir_world = GetPosition().Toglm() + world_fwd * 100.0f;
		glm::vec3 target_dir_local = glm::vec3(0, 0, -1);
		if (target_ != nullptr) {
			float     missile_speed = glm::length(rigid_body_.GetLinearVelocity());
			glm::vec3 missile_pos = GetPosition().Toglm();

			// PREDICT
			target_dir_world =
				GetInterceptPoint(missile_pos, missile_speed, target_->GetPosition(), target_->GetVelocity());

			// Check LOS to target
			float     hit_dist;
			glm::vec3 terrain_normal;
			glm::vec3 to_target = glm::normalize(target_dir_world - missile_pos);
			if (handler.RaycastTerrain(
					missile_pos,
					to_target,
					glm::length(target_dir_world - missile_pos),
					hit_dist,
					terrain_normal
				)) {
				// Aim for approach point if launcher is obscured, but start cutting the corner
				// once we are close enough to the approach point (crested the hill).
				glm::vec3 approach_p = target_->GetApproachPoint();
				glm::vec3 target_p = target_->GetPosition().Toglm();
				float     d_at = glm::distance(approach_p, target_p);
				float     d_ma = glm::distance(missile_pos, approach_p);

				if (d_ma <= d_at && d_at > 1e-4f) {
					float t = d_ma / d_at;
					target_dir_world = glm::mix(target_p, approach_p, t);
				} else {
					target_dir_world = approach_p;
				}
			}

			target_dir_local = WorldToObject(glm::normalize(target_dir_world - missile_pos));
		}

		const auto* terrain_generator = handler.GetTerrainGenerator();
		if (terrain_generator) {
			Vector3 vel_vec = GetVelocity();
			if (vel_vec.MagnitudeSquared() > 1e-6) {
				glm::vec3 origin = GetPosition().Toglm();
				glm::vec3 dir = glm::normalize(vel_vec.Toglm());

				float     hit_dist = 0.0f;
				glm::vec3 terrain_normal;
				if (handler.RaycastTerrain(origin, dir, reaction_distance, hit_dist, terrain_normal)) {
					glm::vec3 local_up = glm::vec3(0.0f, 1.0f, 0.0f);
					auto      away = terrain_normal;
					if (glm::dot(away, local_up) < 0.5f) {
						away = local_up;
					}

					if (target_ != nullptr) {
						glm::vec3 to_target = glm::normalize(target_dir_world - origin);
						away = to_target - (glm::dot(to_target, away)) * away;
						if (glm::length(away) > 1e-4f) {
							away = glm::normalize(away);
						} else {
							away = terrain_normal;
						}
					}

					float distance_factor = 1.0f - (hit_dist / reaction_distance);
					float alignment_with_target = glm::dot(dir, glm::normalize(target_dir_world - origin));
					float target_priority = 1.0f - glm::clamp(alignment_with_target, 0.0f, 1.0f);

					float avoidance_weight = distance_factor;
					if (target_ != nullptr) {
						avoidance_weight *= target_priority * (target_distance / reaction_distance);
						// Further dampen avoidance when very close to target
						if (target_distance < 100.0f) {
							avoidance_weight *= (target_distance / 100.0f);
						}
					}

					glm::vec3 current_desired_dir = (target_ != nullptr) ? glm::normalize(target_dir_world - origin)
																		 : world_fwd;

					glm::vec3 final_desired_dir = glm::normalize(glm::mix(current_desired_dir, away, avoidance_weight));
					target_dir_local = WorldToObject(final_desired_dir);
				}
			}
		}

		glm::vec3 local_forward = glm::vec3(0, 0, -1);

		// Spiral movement that tightens as it approaches target
		float max_spiral = 0.25;
		float spiral_amplitude = max_spiral;
		if (target_ != nullptr) {
			spiral_amplitude = glm::mix(0.0f, max_spiral, std::clamp(target_distance / 300.0f, 0.0f, 1.0f));
		}
		float side = leftHanded_ ? -1.0f : 1.0f;
		target_dir_local.x += sin(lived_ * 2.0f) * side * spiral_amplitude;
		target_dir_local.y += cos(lived_ * 1.5f) * side * spiral_amplitude;

		// Terminal hard swing
		float kP = 60.0f;
		if (target_ != nullptr && target_distance < 80.0f) {
			kP = 250.0f; // Increase gain for terminal swing
		}

		glm::vec3 pid_torque = CalculateSteeringTorque(
			local_forward,
			target_dir_local,
			rigid_body_.GetAngularVelocity(),
			kP,
			glm::mix(0.0f, 5.0f, std::clamp(2 * lived_ / lifetime_, 0.0f, 1.0f))
		);

		rigid_body_.AddRelativeTorque(pid_torque);
	}

	void CatMissile::Explode(const EntityHandler& handler, bool hit_target) {
		if (exploded_)
			return;

		exploded_ = true;
		lived_ = 0.0f;
		SetVelocity(Vector3(0, 0, 0));

		shape_->SetHidden(true);

		if (hit_target && target_) {
			target_->Destroy(handler);
		}

		auto pos = GetPosition();
		handler.EnqueueVisualizerAction([exhaust = exhaust_effect_]() {
			if (exhaust) {
				exhaust->SetLifetime(0.25f);
				exhaust->SetLived(0.0f);
			}
		});

		handler.EnqueueVisualizerAction([this, pos, &handler]() {
			handler.vis->AddFireEffect(
				glm::vec3(pos.x, pos.y, pos.z),
				FireEffectStyle::Explosion,
				glm::vec3(0, 1, 0),
				glm::vec3(0, 0, 0),
				-1,
				5.0f
			);
			this->explode_sound_ =
				handler.vis->AddSoundEffect("assets/rocket_explosion.wav", pos.Toglm(), GetVelocity().Toglm(), 25.0f);
		});
	}

} // namespace Boidsish
