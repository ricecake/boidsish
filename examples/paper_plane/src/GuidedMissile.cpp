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
		glm::vec3 target_dir = target_pos - shooter_pos;
		float     dist = glm::length(target_dir);

		// Simple time-to-impact estimation
		// (You can make this more complex with quadratic equations, but this works for games)
		float time_to_impact = dist / shooter_speed;

		// Predict where the target will be
		return target_pos + (target_vel * time_to_impact);
	}

	GuidedMissile::GuidedMissile(int id, Vector3 pos): Entity<Model>(id, "assets/Missile.obj", true), eng_(rd_()) {
		auto orientation = glm::angleAxis(glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

		auto dist = std::uniform_int_distribution(0, 1);
		auto wobbleDist = std::uniform_real_distribution<float>(0.75f, 1.50f);
		handedness = dist(eng_) ? 1 : -1;
		wobble = wobbleDist(eng_);

		SetPosition(pos.x, pos.y + 0.5f, pos.z);
		rigid_body_.SetOrientation(orientation);
		rigid_body_.SetAngularVelocity(glm::vec3(0, 0, 0));
		rigid_body_.SetLinearVelocity(glm::vec3(0, 100, 0));

		SetTrailLength(100);
		SetTrailRocket(true);
		shape_->SetScale(glm::vec3(0.08f));
		std::dynamic_pointer_cast<Model>(shape_)->SetBaseRotation(
			glm::angleAxis(glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f))
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
		auto [height, norm] = handler.vis->GetTerrainPointPropertiesThreadSafe(pos.x, pos.z);
		if (pos.y < height) {
			Explode(handler, false);
			return;
		}

		// --- Flight Model Constants ---
		const float kLaunchTime = 0.5f;
		const float kMaxSpeed = 170.0f;
		const float kAcceleration = 150.0f;

		if (lived_ < kLaunchTime) {
			rigid_body_.AddRelativeForce(glm::vec3(0, 0, 600));
		} else {
			const float kTurnSpeed = 100.0f;
			const float kDamping = 2.5f;

			auto targets = handler.GetEntitiesByType<PaperPlane>();
			if (!targets.empty()) {
				auto  plane = targets[0];
				auto& r = rigid_body_;

				if ((plane->GetPosition() - GetPosition()).Magnitude() < 10) {
					Explode(handler, true);
					plane->TriggerDamage();
					return;
				}

				r.AddRelativeForce(glm::vec3(0, 0, 1000));

		glm::vec3 target_dir_local = glm::vec3(0, 0, -1);


				glm::vec3 velocity = rigid_body_.GetLinearVelocity();
				glm::vec3 target_vec = (plane->GetPosition() - GetPosition()).Toglm();
				glm::vec3 target_dir_world = glm::normalize(target_vec);

				std::uniform_real_distribution<float> dist(0.25f, 0.5f);
				auto                                  distance = glm::length(target_vec);
				auto                                  distance_scale = log10(distance / 50.0f + 1);
				float                                 error_scale = (1.0f - lived_ / lifetime_) * distance_scale;
				glm::vec3                             right = glm::cross(target_dir_world, glm::vec3(0, 1, 0));
				glm::vec3                             up = glm::cross(right, target_dir_world);
				auto theta = handedness * lived_ * (2.0f + 2 * (1.0f - distance_scale));
				auto offset = 0;//(right * sin(theta / 3) * cos(theta)) + (up * cos(theta / 2) * sin(theta));

				auto adjusted_target_dir_world = glm::normalize(
					target_vec// + wobble * offset * dist(eng_) * distance * error_scale * error_scale
				);

			float missile_speed = glm::length(rigid_body_.GetLinearVelocity());

			// PREDICT
			target_dir_world =
				GetInterceptPoint2(GetPosition(), missile_speed, adjusted_target_dir_world, plane->GetVelocity());
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
					float       hit_dist = 0.0f;

					Vector3 vel_vec = GetVelocity();
					if (vel_vec.MagnitudeSquared() > 1e-6) {
						glm::vec3 origin = {GetPosition().x, GetPosition().y, GetPosition().z};
						glm::vec3 dir = {vel_vec.x, vel_vec.y, vel_vec.z};
						dir = glm::normalize(dir);

						if (terrain_generator->Raycast(origin, dir, reaction_distance, hit_dist)) {
							auto hit_coord = vel_vec.Normalized() * hit_dist;
							auto [terrain_h, terrain_normal] = terrain_generator->pointProperties(
								hit_coord.x,
								hit_coord.z
							);

							const float avoidance_strength = 20.0f;
							const float kUpAlignmentThreshold = 0.5f;
							float force_magnitude = avoidance_strength * (1.0f - ((10 + hit_dist) / reaction_distance));

							glm::vec3 local_up = glm::vec3(0.0f, 1.0f, 0.0f);
							auto      away = terrain_normal;
							if (glm::dot(away, local_up) < kUpAlignmentThreshold) {
								away = local_up;
							}

							target_dir_local = WorldToObject(away);
						away = target_dir_world - (glm::dot(target_dir_world, away)) * away;

					float     distance_factor = 1.0f - (hit_dist / reaction_distance);
					float     alignment_with_target = glm::dot(dir, target_dir_world);
					float     target_priority = 1.0f - glm::clamp(alignment_with_target, 0.0f, 1.0f);
					float     avoidance_weight = distance_factor * target_priority;
					glm::vec3 final_desired_dir = glm::normalize(
						target_dir_world + (away * avoidance_weight * avoidance_strength)
					);
					target_dir_local = WorldToObject(final_desired_dir);
						}
					}
				}

		glm::vec3 local_forward = glm::vec3(0, 0, 1);
		// target_dir_local.x += sin(lived_ * 20.0f) * 0.075f;
		// target_dir_local.y += cos(lived_ * 15.0f) * 0.075f;
		glm::vec3 pid_torque = CalculateSteeringTorque2(
			local_forward,
			target_dir_local,
			rigid_body_.GetAngularVelocity(),
			50.0f, // kP,
			5.0f
			// glm::mix(0.0f, 100.0f, std::clamp(2 * lived_ / lifetime_, 0.0f, 1.0f))
		);

		rigid_body_.AddRelativeTorque(-1*pid_torque);


			}
		}
	}

	void GuidedMissile::Explode(const EntityHandler& handler, bool hit_target) {
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
			SetSize(100);
			SetColor(1, 0, 0, 0.33f);
		}
	}

} // namespace Boidsish
