#include "GuidedMissile.h"

#include "PaperPlane.h"
#include "fire_effect.h"
#include "graphics.h"
#include "terrain_generator.h"
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {
	// Calculates the torque needed to rotate 'current_forward' to align with 'desired_direction'
	// using a PD controller to prevent overshoot.
	glm::vec3 CalculateSteeringTorque2(
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
	GetInterceptPoint2(glm::vec3 shooter_pos, float shooter_speed, glm::vec3 target_pos, glm::vec3 target_vel) {
		glm::vec3 to_target = target_pos - shooter_pos;
		float     dist = glm::length(to_target);

		if (dist < 1e-6f) {
			return target_pos;
		}
		glm::vec3 to_target_dir = to_target / dist;

		// Calculate the closing speed. This is the missile's speed towards the target
		// minus the target's speed away from the missile.
		float target_speed_away = glm::dot(target_vel, to_target_dir);
		float closing_speed = shooter_speed - target_speed_away;

		// If the target is faster and moving away, we'll never catch it.
		// In this case, just aim at the target's current position.
		if (closing_speed <= 0.0f) {
			return target_pos;
		}

		// Estimate time to impact
		float time_to_impact = dist / closing_speed;

		// To prevent over-prediction (which can cause instability), cap the prediction time.
		// A value of 1.0 seconds seems reasonable; it means we won't predict more than
		// one second into the future.
		const float max_prediction_time = 1.0f;
		time_to_impact = glm::min(time_to_impact, max_prediction_time);

		// Predict where the target will be
		return target_pos + (target_vel * time_to_impact);
	}

	GuidedMissile::GuidedMissile(int id, Vector3 pos): Entity<Model>(id, "assets/Missile.obj", true), eng_(rd_()) {
		auto orientation = glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

		auto dist = std::uniform_int_distribution(0, 1);
		auto wobbleDist = std::uniform_real_distribution<float>(0.75f, 1.50f);
		handedness = dist(eng_) ? 1 : -1;
		wobble = wobbleDist(eng_);

		SetPosition(pos.x, pos.y, pos.z);
		rigid_body_.SetOrientation(orientation);
		rigid_body_.SetAngularVelocity(glm::vec3(0, 0, 0));
		rigid_body_.SetLinearVelocity(glm::vec3(0, 0, -100));

		SetTrailLength(100);
		SetTrailRocket(true);
		shape_->SetScale(glm::vec3(0.08f));
		std::dynamic_pointer_cast<Model>(shape_)->SetBaseRotation(
			glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f))
		);
		shape_->SetInstanced(true);
	}

	void GuidedMissile::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		lived_ += delta_time;
		auto pos = GetPosition();

		if (!launch_sound_) {
			launch_sound_ = handler.vis
								->AddSoundEffect("assets/sam_launch.wav", pos.Toglm(), GetVelocity().Toglm(), 30.0f);
		}

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

		// --- Flight Model Constants ---
		const float kLaunchTime = 0.5f;
		const float kMaxSpeed = 170.0f;
		const float kAcceleration = 150.0f;

		if (lived_ < kLaunchTime) {
			rigid_body_.AddRelativeForce(glm::vec3(0, 0, -600));
		} else {
			auto [height, norm] = handler.vis->GetTerrainPointPropertiesThreadSafe(pos.x, pos.z);
			if (pos.y < height) {
				Explode(handler, false);
				return;
			}
			const float kTurnSpeed = 100.0f;
			const float kDamping = 2.5f;

			auto targets = handler.GetEntitiesByType<PaperPlane>();
			if (!targets.empty()) {
				PaperPlane*                 closest = nullptr;
				float                       min_dist_sq = std::numeric_limits<float>::max();
				for (auto p : targets) {
					float d2 = (p->GetPosition() - GetPosition()).MagnitudeSquared();
					if (d2 < min_dist_sq) {
						min_dist_sq = d2;
						closest = p;
					}
				}
				auto plane = closest;
				target_ = plane;
				auto& r = rigid_body_;

				if ((plane->GetPosition() - GetPosition()).Magnitude() < 10) {
					Explode(handler, true);
					return;
				}

				r.AddRelativeForce(glm::vec3(0, 0, -1000));

				glm::vec3 target_dir_local = glm::vec3(0, 0, -1);

				glm::vec3 velocity = rigid_body_.GetLinearVelocity();
				glm::vec3 target_vec = (plane->GetPosition() - GetPosition()).Toglm();
				glm::vec3 target_dir_world = glm::normalize(target_vec);

				float missile_speed = glm::length(rigid_body_.GetLinearVelocity());

				// PREDICT
				target_dir_world =
					GetInterceptPoint2(GetPosition(), missile_speed, plane->GetPosition(), plane->GetVelocity());

				target_dir_local = WorldToObject(glm::normalize(target_dir_world - GetPosition()));

				/*
				    use something like this for spiral attacker!
				    float error_scale = 1;//1.0f - lived_/lifetime_;
				    // log10(x/10+1)/2
				    error_scale *= log10(glm::length(target_vec)/25.0f+1);
				    glm::vec3 right = glm::cross(target_dir_world, glm::vec3(0,1,0));
				    glm::vec3 up = glm::cross(right, target_dir_world);
				    auto theta = lived_ * 5.0f/error_scale;
				    auto offset=(right*cos(theta))+(up*sin(theta));

				    auto adjusted_target_dir_world = glm::normalize(target_vec + offset*200.0f*error_scale);

				    glm::vec3 target_dir_local = WorldToObject(adjusted_target_dir_world);
				    glm::vec3 P = glm::vec3(0, 0, 1);
				    glm::vec3 torque = glm::cross(P, target_dir_local);
				*/

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

						float hit_dist = 0.0f;
						if (terrain_generator->Raycast(origin, dir, reaction_distance, hit_dist)) {
							auto hit_coord = vel_vec.Normalized() * hit_dist;
							auto [terrain_h, terrain_normal] = terrain_generator->pointProperties(
								hit_coord.x,
								hit_coord.z
							);

							glm::vec3 local_up = glm::vec3(0.0f, 1.0f, 0.0f);
							auto      away = terrain_normal;
							if (glm::dot(away, local_up) < kUpAlignmentThreshold) {
								away = local_up;
							}

							away = target_dir_world - (glm::dot(target_dir_world, away)) * away;

							float     distance_factor = 1.0f - (hit_dist / reaction_distance);
							float     alignment_with_target = glm::dot(dir, target_dir_world);
							float     target_priority = 1.0f - glm::clamp(alignment_with_target, 0.0f, 1.0f);
							float     avoidance_weight = distance_factor * target_priority;
							glm::vec3 final_desired_dir = glm::normalize(
								target_dir_world + (away * avoidance_weight * kAvoidanceStrength)
							);
							target_dir_local = WorldToObject(final_desired_dir);
						}
					}
				}

				glm::vec3 local_forward = glm::vec3(0, 0, -1);
				target_dir_local.x += sin(handedness * lived_ * 20.0f) * 0.075f;
				target_dir_local.y += cos(handedness * lived_ * 15.0f) * 0.075f;
				glm::vec3 pid_torque = CalculateSteeringTorque2(
					local_forward,
					target_dir_local,
					rigid_body_.GetAngularVelocity(),
					50.0f, // kP,
					glm::mix(0.0f, 5.0f, std::clamp(2 * lived_ / lifetime_, 0.0f, 1.0f))
				);

				rigid_body_.AddRelativeTorque(pid_torque);
			}
		}
	}

	void GuidedMissile::Explode(const EntityHandler& handler, bool hit_target) {
		if (exploded_)
			return;

		shape_->SetHidden(true);
		auto pos = GetPosition();
		handler.EnqueueVisualizerAction([=, &handler]() {
			handler.vis->AddFireEffect(
				glm::vec3(pos.x, pos.y, pos.z),
				FireEffectStyle::Explosion,
				glm::vec3(0, 1, 0),
				glm::vec3(0, 0, 0),
				-1,
				2.0f
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
							 ->AddSoundEffect("assets/rocket_explosion.wav", pos.Toglm(), GetVelocity().Toglm(), 20.0f);

		if (hit_target) {
			target_->TriggerDamage();
		}
	}

} // namespace Boidsish
