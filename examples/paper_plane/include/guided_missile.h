#pragma once

// #include <iostream>
// #include <memory>
// #include <random>
// #include <set>
// #include <vector>

// #include "arrow.h"
// #include "bomb.h"
// #include "dot.h"
// #include "emplacements.h"
// #include "field.h"
#include "graphics.h"
// #include "handler.h"
// #include "hud.h"
// #include "logger.h"
// #include "model.h"
#include "plane.h"
#include "guided_missile.h"
// #include "spatial_entity_handler.h"
// #include "terrain_generator.h"
// #include <GLFW/glfw3.h>
// #include <fire_effect.h>
// #include <glm/gtc/constants.hpp>
// #include <glm/gtc/quaternion.hpp>
// #include <glm/gtx/quaternion.hpp>

using namespace Boidsish;

class GuidedMissile: public Entity<Model> {
public:
	GuidedMissile(int id = 0, Vector3 pos = {0, 0, 0}):
		Entity<Model>(id, "assets/Missile.obj", true),
		rotational_velocity_(glm::vec3(0.0f)),
		forward_speed_(0.0f),
		eng_(rd_()) {
		SetPosition(pos.x, pos.y, pos.z);
		SetVelocity(0, 0, 0);
		SetTrailLength(500);
		SetTrailRocket(true);
		shape_->SetScale(glm::vec3(0.08f));
		std::dynamic_pointer_cast<Model>(shape_)->SetBaseRotation(
			glm::angleAxis(glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f))
		);

		UpdateShape();
	}

	void UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		lived_ += delta_time;
		auto pos = GetPosition();

		// --- Lifetime & Explosion ---
		if (exploded_) {
			if (lived_ >= kExplosionDisplayTime) {
				handler.QueueRemoveEntity(id_);
			}
			return;
		}

		if (lived_ >= lifetime_) {
			Explode(handler, false); // Explode at end of life
			return;
		}

		// --- Manage Exhaust Fire Effect ---
		// This is done via lambda capture to ensure the effect is updated
		// and eventually terminated correctly, even if the missile is destroyed.
		// if (exhaust_effect_ == nullptr) {
		// 	handler.EnqueueVisualizerAction([this, &handler, pos]() {
		// 		exhaust_effect_ = handler.vis->AddFireEffect(
		// 			glm::vec3(pos.x, pos.y, pos.z),
		// 			FireEffectStyle::MissileExhaust,
		// 			orientation_ * glm::vec3(0, 0, -1)
		// 		);
		// 	});
		// } else {
		// 	handler.EnqueueVisualizerAction([this, pos]() {
		// 		if (exhaust_effect_) {
		// 			exhaust_effect_->SetPosition({pos.x, pos.y, pos.z});
		// 			exhaust_effect_->SetDirection(orientation_ * glm::vec3(0, 0, -1));
		// 		}
		// 	});
		// }

		// --- Flight Model Constants ---
		const float kLaunchTime = 0.5f;
		const float kMaxSpeed = 170.0f;
		const float kAcceleration = 150.0f;

		// --- Launch Phase ---
		if (lived_ < kLaunchTime) {
			// Set orientation to point straight up.
			// The model's "forward" is -Z, so we rotate it to point along +Y.
			orientation_ = glm::angleAxis(glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

			// Accelerate
			forward_speed_ += kAcceleration * delta_time;
			if (forward_speed_ > kMaxSpeed) {
				forward_speed_ = kMaxSpeed;
			}

		}

		else {
			// --- Guidance Phase ---
			const float kTurnSpeed = 4.0f;
			const float kDamping = 2.5f;

			auto targets = handler.GetEntitiesByType<PaperPlane>();
			if (targets.empty()) {
				// No target, fly straight.
				rotational_velocity_ = glm::vec3(0.0f);
			} else {
				auto plane = targets[0];

				// --- Proximity Detonation ---
				if ((plane->GetPosition() - GetPosition()).Magnitude() < 10) {
					Explode(handler, true);
					plane->TriggerDamage();
					return;
				}

				// --- Proportional Guidance ---
				// 1. Get world-space direction to target
				Vector3   target_vec = (plane->GetPosition() - GetPosition()).Normalized();
				glm::vec3 target_dir_world = glm::vec3(target_vec.x, target_vec.y, target_vec.z);

				// 2. Convert to missile's local space
				glm::vec3 target_dir_local = glm::inverse(orientation_) * target_dir_world;

				// 3. Calculate target rotational velocity
				//    The local target's X component drives yaw, Y component drives pitch.
				//    This creates a proportional control: the further off-axis the target is, the stronger the turn.
				glm::vec3 target_rot_velocity = glm::vec3(0.0f);
				target_rot_velocity.y = target_dir_local.x * kTurnSpeed;  // Yaw
				target_rot_velocity.x = -target_dir_local.y * kTurnSpeed; // Pitch

				// 4. Damp and apply rotational velocity
				rotational_velocity_ += (target_rot_velocity - rotational_velocity_) * kDamping * delta_time;

				if (lived_ <= 1.5f) {
					std::uniform_real_distribution<float> dist(-4.0f, 4.0f);
					glm::vec3                             error_vector(0.1f * dist(eng_), dist(eng_), 0);
					rotational_velocity_ += error_vector * delta_time;
				}
				// --- Terrain Avoidance ---
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

							// We have a potential collision, apply avoidance force
							const float avoidance_strength = 20.0f;
							const float kUpAlignmentThreshold = 0.5f;
							float force_magnitude = avoidance_strength * (1.0f - ((10 + hit_dist) / reaction_distance));

							glm::vec3 local_up = glm::vec3(0.0f, 1.0f, 0.0f);
							auto      away = terrain_normal;
							if (glm::dot(away, local_up) < kUpAlignmentThreshold) {
								away = local_up;
							}
							glm::vec3 avoidance_force = away * force_magnitude;
							glm::vec3 avoidance_local = glm::inverse(orientation_) * avoidance_force;
							rotational_velocity_.y += avoidance_local.x * avoidance_strength * delta_time; // Yaw
							rotational_velocity_.x += avoidance_local.y * avoidance_strength * delta_time; // Pitch
						}
					}
				}
			}
		}

		// --- Update Orientation ---
		glm::quat pitch_delta = glm::angleAxis(rotational_velocity_.x * delta_time, glm::vec3(1.0f, 0.0f, 0.0f));
		glm::quat yaw_delta = glm::angleAxis(rotational_velocity_.y * delta_time, glm::vec3(0.0f, 1.0f, 0.0f));
		orientation_ = glm::normalize(orientation_ * pitch_delta * yaw_delta);

		// --- Update Velocity and Position ---
		glm::vec3 forward_dir = orientation_ * glm::vec3(0.0f, 0.0f, 1.0f);
		glm::vec3 new_velocity = forward_dir * forward_speed_;
		SetVelocity(Vector3(new_velocity.x, new_velocity.y, new_velocity.z));
	}

	void UpdateShape() override {
		// First, call the base implementation
		Entity<Model>::UpdateShape();
		// Then, apply our specific orientation that includes roll
		if (shape_) {
			shape_->SetRotation(orientation_);
		}
	}

	void Explode(const EntityHandler& handler, bool hit_target) {
		if (exploded_)
			return;

		// --- Create Explosion Effect ---
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

		// --- Clean Up Exhaust ---
		// Give the exhaust a short lifetime to fizzle out
		handler.EnqueueVisualizerAction([exhaust = exhaust_effect_]() {
			if (exhaust) {
				exhaust->SetLifetime(0.25f);
				exhaust->SetLived(0.0f);
			}
		});

		exploded_ = true;
		lived_ = 0.0f;
		SetVelocity(Vector3(0, 0, 0));

		if (hit_target) {
			SetSize(100);
			SetColor(1, 0, 0, 0.33f);
		}
	}

private:
	// Constants
	static constexpr float lifetime_ = 12.0f;
	static constexpr float kExplosionDisplayTime = 2.0f;
	// State
	float                       lived_ = 0.0f;
	bool                        exploded_ = false;
	std::shared_ptr<FireEffect> exhaust_effect_ = nullptr;

	// Flight model
	glm::quat          orientation_;
	glm::vec3          rotational_velocity_; // x: pitch, y: yaw, z: roll
	float              forward_speed_;
	std::random_device rd_;
	std::mt19937       eng_;
};

