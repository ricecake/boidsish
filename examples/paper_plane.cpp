#include <iostream>
#include <memory>
#include <random>
#include <set>
#include <vector>

#include "arrow.h"
#include "dot.h"
#include "field.h"
#include "graphics.h"
#include "hud.h"
#include "logger.h"
#include "model.h"
#include "spatial_entity_handler.h"
#include "terrain_generator.h"
#include <GLFW/glfw3.h>
#include <fire_effect.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

using namespace Boidsish;

std::vector<const Terrain*>
get_neighbors(const Terrain* chunk, const std::vector<std::shared_ptr<Terrain>>& all_chunks) {
	std::vector<const Terrain*> neighbors;
	int                         center_x = chunk->GetX();
	int                         center_z = chunk->GetZ();
	float                       chunk_size = std::sqrt(chunk->proxy.radiusSq) * 2.0f;

	for (const auto& other_chunk_ptr : all_chunks) {
		const Terrain* other_chunk = other_chunk_ptr.get();
		if (other_chunk == chunk) {
			continue;
		}

		int other_x = other_chunk->GetX();
		int other_z = other_chunk->GetZ();

		bool is_neighbor = std::abs(other_x - center_x) <= chunk_size && std::abs(other_z - center_z) <= chunk_size;

		if (is_neighbor) {
			neighbors.push_back(other_chunk);
		}
	}

	return neighbors;
}

class CatMissile;
class CatBomb;
class GuidedMissile;
class PaperPlane;

static int selected_weapon = 0;

struct PaperPlaneInputController {
	bool pitch_up = false;
	bool pitch_down = false;
	bool yaw_left = false;
	bool yaw_right = false;
	bool roll_left = false;
	bool roll_right = false;
	bool boost = false;
	bool brake = false;
	bool fire = false;
};

class PaperPlane: public Entity<Model> {
public:
	PaperPlane(int id = 0):
		Entity<Model>(id, "assets/Mesh_Cat.obj", true),
		orientation_(glm::quat(1.0f, 0.0f, 0.0f, 0.0f)),
		rotational_velocity_(glm::vec3(0.0f)),
		forward_speed_(20.0f) {
		SetTrailLength(150);
		SetTrailIridescence(true);

		SetColor(1.0f, 0.5f, 0.0f);
		shape_->SetScale(glm::vec3(0.04f));
		std::dynamic_pointer_cast<Model>(shape_)->SetBaseRotation(
			glm::angleAxis(glm::radians(-180.0f), glm::vec3(0.0f, 1.0f, 0.0f))
		);
		SetPosition(0, 4, 0);

		// Initial velocity for a nice takeoff
		SetVelocity(Vector3(0, 0, 20));

		// Correct the initial orientation to match the model's alignment
		orientation_ = glm::angleAxis(glm::radians(25.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		UpdateShape();
	}

	void SetController(std::shared_ptr<PaperPlaneInputController> controller) { controller_ = controller; }

	void UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		if (!controller_)
			return;

		// --- Constants for flight model ---
		const float kPitchSpeed = 1.5f;
		const float kYawSpeed = 1.5f;
		const float kRollSpeed = 3.0f;
		const float kCoordinatedTurnFactor = 0.8f;
		const float kAutoLevelSpeed = 1.5f;
		const float kDamping = 2.5f;

		const float kBaseSpeed = 60.0f;
		const float kBoostSpeed = 90.0f;
		const float kBreakSpeed = 10.0f;
		const float kBoostAcceleration = 120.0f;
		const float kSpeedDecay = 20.0f;

		// --- Handle Rotational Input ---
		glm::vec3 target_rot_velocity = glm::vec3(0.0f);
		if (controller_->pitch_up)
			target_rot_velocity.x += kPitchSpeed;
		if (controller_->pitch_down)
			target_rot_velocity.x -= kPitchSpeed;
		if (controller_->yaw_left)
			target_rot_velocity.y += kYawSpeed;
		if (controller_->yaw_right)
			target_rot_velocity.y -= kYawSpeed;
		if (controller_->roll_left)
			target_rot_velocity.z += kRollSpeed;
		if (controller_->roll_right)
			target_rot_velocity.z -= kRollSpeed;

		// --- Coordinated Turn (Banking) ---
		// Automatically roll when yawing
		target_rot_velocity.z += target_rot_velocity.y * kCoordinatedTurnFactor;

		// --- Auto-leveling ---
		if (!controller_->pitch_up && !controller_->pitch_down && !controller_->yaw_left && !controller_->yaw_right &&
		    !controller_->roll_left && !controller_->roll_right) {
			// This robust auto-leveling logic finds the shortest rotational path to bring the plane upright and level
			// simultaneously.

			// --- Get Orientation Vectors ---
			glm::vec3 world_up = glm::vec3(0.0f, 1.0f, 0.0f);
			glm::vec3 plane_forward_world = orientation_ * glm::vec3(0.0f, 0.0f, -1.0f);
			glm::vec3 world_up_in_local = glm::inverse(orientation_) * world_up;

			// --- Calculate Pitch Error ---
			// Project the plane's forward vector onto the world's XZ plane to get a horizon-level vector.
			// The angle between the actual forward vector and this projected vector is our pitch error.
			glm::vec3 forward_on_horizon = glm::normalize(
				glm::vec3(plane_forward_world.x, 0.0f, plane_forward_world.z)
			);
			float pitch_error = glm::asin(glm::dot(plane_forward_world, world_up));

			// --- Calculate Roll Error ---
			// The angle of the world's 'up' vector projected onto our local YZ plane gives us the roll error.
			// atan2 provides the shortest angle, correctly handling inverted flight.
			float roll_error = atan2(world_up_in_local.x, world_up_in_local.y);

			// --- Handle Vertical Flight Edge Case ---
			// If the plane is pointing nearly straight up or down, the concept of "roll" is unstable.
			// In this case, we disable roll correction and focus on pitching back to the horizon.
			// The threshold (0.99) corresponds to about 8 degrees from vertical.
			if (abs(glm::dot(plane_forward_world, world_up)) > 0.99f) {
				roll_error = 0.0f;
			}

			// --- Apply Proportional Corrections ---
			// Apply forces proportional to the error angles. This makes the correction smooth
			// and ensures both roll and pitch complete at roughly the same time.
			target_rot_velocity.x -= pitch_error * kAutoLevelSpeed;
			target_rot_velocity.z -= roll_error * kAutoLevelSpeed;
		}

		// --- Apply Damping and Update Rotational Velocity ---
		// Lerp towards the target velocity to create a smooth, responsive feel
		rotational_velocity_ += (target_rot_velocity - rotational_velocity_) * kDamping * delta_time;

		// --- Update Orientation ---
		// Create delta rotations for pitch, yaw, and roll in the plane's local space.
		glm::quat pitch_delta = glm::angleAxis(rotational_velocity_.x * delta_time, glm::vec3(1.0f, 0.0f, 0.0f));
		glm::quat yaw_delta = glm::angleAxis(rotational_velocity_.y * delta_time, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::quat roll_delta = glm::angleAxis(rotational_velocity_.z * delta_time, glm::vec3(0.0f, 0.0f, 1.0f));

		// Combine the deltas and apply to the main orientation (post-multiplication for local-space rotation)
		orientation_ = glm::normalize(orientation_ * pitch_delta * yaw_delta * roll_delta);

		// --- Update Speed (Boost & Decay) ---
		if (controller_->boost) {
			forward_speed_ += kBoostAcceleration * delta_time;
			if (forward_speed_ > kBoostSpeed)
				forward_speed_ = kBoostSpeed;
		} else if (controller_->brake) {
			forward_speed_ -= kBoostAcceleration * delta_time;
			if (forward_speed_ < kBreakSpeed)
				forward_speed_ = kBreakSpeed;

		} else {
			if (forward_speed_ > kBaseSpeed) {
				forward_speed_ -= kSpeedDecay * delta_time;
				if (forward_speed_ < kBaseSpeed)
					forward_speed_ = kBaseSpeed;
			} else if (forward_speed_ < kBaseSpeed) {
				forward_speed_ += kSpeedDecay * delta_time;
				if (forward_speed_ > kBaseSpeed)
					forward_speed_ = kBaseSpeed;
			}
		}

		// --- Update Velocity and Position ---
		// The model's "forward" is along the negative Z-axis in its local space
		glm::vec3 forward_dir = orientation_ * glm::vec3(0.0f, 0.0f, -1.0f);
		glm::vec3 new_velocity = forward_dir * forward_speed_;

		SetVelocity(Vector3(new_velocity.x, new_velocity.y, new_velocity.z));

		time_to_fire -= delta_time;
		if (controller_->fire && time_to_fire <= 0) {
			switch (selected_weapon) {
			case 0: {
				handler.QueueAddEntity<CatMissile>(
					GetPosition(),
					orientation_,
					orientation_ * glm::vec3(fire_left ? -1 : 1, -1, 0),
					GetVelocity()
				);
				time_to_fire = 0.25f;
				fire_left = !fire_left;
				break;
			}
			case 1: {
				handler.QueueAddEntity<CatBomb>(GetPosition(), orientation_ * glm::vec3(0, -1, 0), GetVelocity());
				time_to_fire = 0.25f;
				break;
			}
			}
		}
	}

	void UpdateShape() override {
		// First, call the base implementation
		Entity<Model>::UpdateShape();
		// Then, apply our specific orientation that includes roll
		if (shape_) {
			shape_->SetRotation(orientation_);
		}
	}

	void TriggerDamage() {
		health -= 5;
		damage_pending_++;
	}

	bool IsDamagePending() { return bool(damage_pending_); }

	void AcknowledgeDamage() { damage_pending_--; }

	float GetHealth() const { return health; }

	float GetShield() const { return shield; }

private:
	std::shared_ptr<PaperPlaneInputController> controller_;
	glm::quat                                  orientation_;
	glm::vec3                                  rotational_velocity_; // x: pitch, y: yaw, z: roll
	float                                      forward_speed_;
	float                                      time_to_fire = 0.25f;
	bool                                       fire_left = true;
	int                                        damage_pending_ = false;
	float                                      health = 100.0f;
	float                                      shield = 100.0f;
};

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

class GuidedMissileLauncher: public Entity<Model> {
public:
	GuidedMissileLauncher(int id, Vector3 pos, glm::quat orientation):
		Entity<Model>(id, "assets/utah_teapot.obj", false), eng_(rd_()) {
		SetPosition(pos.x, pos.y, pos.z);
		shape_->SetScale(glm::vec3(2.0f)); // Set a visible scale
		shape_->SetBaseRotation(glm::angleAxis(glm::radians(0.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
		shape_->SetRotation(orientation);
		// shape_->SetBaseRotation(orientation);
		std::uniform_real_distribution<float> dist(4.0f, 8.0f);
		fire_interval_ = dist(eng_);

		UpdateShape();
	}

	void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override {
		time_since_last_fire_ += delta_time;
		if (time_since_last_fire_ < fire_interval_) {
			return;
		}

		auto planes = handler.GetEntitiesByType<PaperPlane>();
		if (planes.empty())
			return;
		auto plane = planes[0];

		// Check distance to plane
		float distance_to_plane = (plane->GetPosition() - GetPosition()).Magnitude();
		if (distance_to_plane > 500.0f) {
			return;
		}

		auto pos = GetPosition();
		// Calculate firing probability based on altitude
		auto  ppos = plane->GetPosition();
		auto  pvel = plane->GetVelocity();
		float max_h = handler.vis->GetTerrainMaxHeight();

		if (max_h <= 0.0f)
			max_h = 200.0f; // Fallback

		float start_h = 60.0f;
		float extreme_h = 3.0f * max_h;

		if (ppos.y < start_h)
			return;

		const float p_min = 0.5f;
		const float p_max = 10.0f;

		auto directionWeight = glm::dot(glm::normalize(pvel), glm::normalize(pos - ppos));

		float norm_alt = (ppos.y - start_h) / (extreme_h - start_h);
		norm_alt = std::min(std::max(norm_alt, 0.0f), 1.0f);

		float missiles_per_second = p_min + (p_max - p_min) * norm_alt;
		float fire_probability_this_frame = missiles_per_second * directionWeight * delta_time;

		std::uniform_real_distribution<float> dist(0.0f, 1.0f);
		if (dist(eng_) < fire_probability_this_frame) {
			handler.QueueAddEntity<GuidedMissile>(GetPosition());
			time_since_last_fire_ = 0.0f;
			// Set a new random interval for the next shot
			std::uniform_real_distribution<float> new_dist(4.0f, 8.0f);
			fire_interval_ = new_dist(eng_);
		}
	}

	float time_since_last_fire_ = 0.0f;
	float fire_interval_ = 5.0f; // Fire every 5 seconds, will be randomized

	std::random_device rd_;
	std::mt19937       eng_;
};

class CatMissile: public Entity<Model> {
public:
	CatMissile(
		int       id = 0,
		Vector3   pos = {0, 0, 0},
		glm::quat orientation = {0, {0, 0, 0}},
		glm::vec3 dir = {0, 0, 0},
		Vector3   vel = {0, 0, 0}
	):
		Entity<Model>(id, "assets/Missile.obj", true),
		rotational_velocity_(glm::vec3(0.0f)),
		forward_speed_(0.0f),
		eng_(rd_()),
		orientation_(orientation) {
		SetOrientToVelocity(false);
		SetPosition(pos.x, pos.y, pos.z);
		auto netVelocity = glm::vec3(vel.x, vel.y, vel.z) + 5.0f * glm::normalize(glm::vec3(dir.x, dir.y, dir.z));
		SetVelocity(netVelocity.x, netVelocity.y, netVelocity.z);

		SetTrailLength(0);
		SetTrailRocket(false);
		shape_->SetScale(glm::vec3(0.05f));
		std::dynamic_pointer_cast<Model>(shape_)->SetBaseRotation(
			glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f))
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

		// --- Flight Model Constants ---
		const float kLaunchTime = 1.0f;
		const float kMaxSpeed = 150.0f;
		const float kAcceleration = 150.0f;

		// --- Launch Phase ---
		if (lived_ < kLaunchTime) {
			auto velo = GetVelocity();
			velo += Vector3(0, -0.07f, 0);
			SetVelocity(velo);
			return;
		} else {
			// --- Post-Launch ---
			if (!fired_) {
				SetTrailLength(500);
				SetTrailRocket(true);
				SetOrientToVelocity(true);
				fired_ = true;
			}

			// // --- Manage Exhaust Fire Effect ---
			// if (exhaust_effect_ == nullptr) {
			// 	handler.EnqueueVisualizerAction([this, &handler, pos]() {
			// 		exhaust_effect_ = handler.vis->AddFireEffect(
			// 			glm::vec3(pos.x, pos.y, pos.z),
			// 			FireEffectStyle::MissileExhaust,
			// 			orientation_ * glm::vec3(0, 0, 1)
			// 		);
			// 	});
			// } else {
			// 	handler.EnqueueVisualizerAction([this, pos]() {
			// 		if (exhaust_effect_) {
			// 			exhaust_effect_->SetPosition(glm::vec3(pos.x, pos.y, pos.z));
			// 			exhaust_effect_->SetDirection(orientation_ * glm::vec3(0, 0, 1));
			// 		}
			// 	});
			// }

			forward_speed_ += kAcceleration * delta_time;
			if (forward_speed_ > kMaxSpeed) {
				forward_speed_ = kMaxSpeed;
			}
			// --- Guidance Phase ---
			const float kTurnSpeed = 4.0f;
			const float kDamping = 2.5f;

			auto targets = handler.GetEntitiesByType<PaperPlane>();
			targets.clear();
			if (targets.empty()) {
				// No target, fly straight.
				rotational_velocity_ = glm::vec3(0.0f);
			} else {
				auto plane = targets[0];

				// --- Proximity Detonation ---
				if ((plane->GetPosition() - GetPosition()).Magnitude() < 10) {
					Explode(handler, true);
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
			}
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
						auto [terrain_h, terrain_normal] = terrain_generator->pointProperties(hit_coord.x, hit_coord.z);

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

		// --- Update Orientation ---
		glm::quat pitch_delta = glm::angleAxis(rotational_velocity_.x * delta_time, glm::vec3(1.0f, 0.0f, 0.0f));
		glm::quat yaw_delta = glm::angleAxis(rotational_velocity_.y * delta_time, glm::vec3(0.0f, 1.0f, 0.0f));
		orientation_ = glm::normalize(orientation_ * pitch_delta * yaw_delta);

		// --- Update Velocity and Position ---
		glm::vec3 forward_dir = orientation_ * glm::vec3(0.0f, 0.0f, -1.0f);
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
				5.0f // lifetime
			);
		});

		// --- Clean Up Exhaust ---
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
	bool                        fired_ = false;
	std::shared_ptr<FireEffect> exhaust_effect_ = nullptr;

	// Flight model
	glm::quat          orientation_;
	glm::vec3          rotational_velocity_; // x: pitch, y: yaw, z: roll
	float              forward_speed_;
	std::random_device rd_;
	std::mt19937       eng_;
};

class CatBomb: public Entity<Model> {
public:
	CatBomb(int id = 0, Vector3 pos = {0, 0, 0}, glm::vec3 dir = {0, 0, 0}, Vector3 vel = {0, 0, 0}):
		Entity<Model>(id, "assets/bomb_shading_v005.obj", true) {
		SetOrientToVelocity(true);
		SetPosition(pos.x, pos.y, pos.z);
		auto netVelocity = glm::vec3(vel.x, vel.y, vel.z) + 2.5f * glm::normalize(glm::vec3(dir.x, dir.y, dir.z));
		SetVelocity(netVelocity.x, netVelocity.y, netVelocity.z);

		SetTrailLength(50);
		shape_->SetScale(glm::vec3(0.01f));
		std::dynamic_pointer_cast<Model>(shape_)->SetBaseRotation(
			glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f))
		);
	}

	void UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		auto pos = GetPosition();
		lived_ += delta_time;

		if (exploded_) {
			// If exploded, just wait to be removed
			if (lived_ >= kExplosionDisplayTime) {
				handler.QueueRemoveEntity(id_);
			}
			return;
		}

		// --- Ground/Terrain Collision ---
		auto [height, norm] = handler.vis->GetTerrainPointProperties(pos.x, pos.z);
		if (pos.y <= height) {
			Explode(handler);
			return;
		}

		// --- Gravity ---
		auto velo = GetVelocity();
		velo += Vector3(0, -kGravity, 0);
		SetVelocity(velo);
	}

private:
	void Explode(const EntityHandler& handler) {
		if (exploded_)
			return;

		auto pos = GetPosition();
		handler.EnqueueVisualizerAction([=, &handler]() {
			handler.vis->AddFireEffect(
				glm::vec3(pos.x, pos.y, pos.z),
				FireEffectStyle::Explosion,
				glm::vec3(0, 1, 0), // direction
				glm::vec3(0, 0, 0), // velocity
				-1,                 // max_particles
				2.0f                // lifetime
			);
		});

		exploded_ = true;
		lived_ = 0.0f; // Reset lived timer for explosion phase
		SetVelocity(Vector3(0, 0, 0));
		SetTrailLength(0); // Stop emitting trail
	}

	// Constants
	static constexpr float kGravity = 0.15f;
	static constexpr float kExplosionDisplayTime = 2.0f; // How long the bomb object sticks around after exploding

	// State
	float lived_ = 0.0f;
	bool  exploded_ = false;
};

class MakeBranchAttractor {
private:
	std::random_device                    rd;
	std::mt19937                          eng;
	std::uniform_real_distribution<float> x;
	std::uniform_real_distribution<float> y;
	std::uniform_real_distribution<float> z;

public:
	MakeBranchAttractor(): eng(rd()), x(-1, 1), y(0, 1), z(-1, 1) {}

	Vector3 operator()(float r) { return r * Vector3(x(eng), y(eng), z(eng)).Normalized(); }
};

static auto missilePicker = MakeBranchAttractor();

class PaperPlaneHandler: public SpatialEntityHandler {
public:
	PaperPlaneHandler(task_thread_pool::task_thread_pool& thread_pool):
		SpatialEntityHandler(thread_pool), eng_(rd_()) {}

	void PreTimestep(float time, float delta_time) {
		if (damage_timer_ > 0.0f) {
			damage_timer_ -= delta_time;
			if (damage_timer_ <= 0.0f) {
				vis->TogglePostProcessingEffect("Glitch", false);
				vis->TogglePostProcessingEffect("Time Stutter", false);
			}
		}

		// --- Guided Missile Launcher Spawning/Despawning ---
		if (vis && vis->GetTerrainGenerator()) {
			const auto&              visible_chunks = vis->GetTerrainGenerator()->getVisibleChunks();
			std::set<const Terrain*> visible_chunk_set;
			// --- Pre-populate forbidden zones from existing launchers ---
			std::set<const Terrain*> forbidden_chunks;
			for (const auto& pair : spawned_launchers_) {
				const Terrain* existing_chunk = pair.first;
				auto           neighbors = get_neighbors(existing_chunk, visible_chunks);
				forbidden_chunks.insert(neighbors.begin(), neighbors.end());
				forbidden_chunks.insert(existing_chunk);
			}

			// --- Pass 1: Candidate Gathering ---
			struct SpawnCandidate {
				const Terrain* chunk;
				glm::vec3      point;
				float          height;
			};

			std::vector<SpawnCandidate> candidates;
			std::set<const Terrain*>    processed_chunks;

			for (const auto& chunk_ptr : visible_chunks) {
				const Terrain* chunk = chunk_ptr.get();
				visible_chunk_set.insert(chunk);
				if (processed_chunks.count(chunk)) {
					continue;
				}

				auto                        neighbors = get_neighbors(chunk, visible_chunks);
				std::vector<const Terrain*> current_grid = neighbors;
				current_grid.push_back(chunk);

				const Terrain* best_chunk = nullptr;
				glm::vec3      highest_point = {0, -std::numeric_limits<float>::infinity(), 0};

				for (const auto& grid_chunk : current_grid) {
					if (grid_chunk->proxy.highestPoint.y > highest_point.y) {
						highest_point = grid_chunk->proxy.highestPoint;
						best_chunk = grid_chunk;
					}
					processed_chunks.insert(grid_chunk);
				}

				if (best_chunk) {
					candidates.push_back({best_chunk, highest_point, highest_point.y});
				}
			}

			// --- Pass 2: Greedy Placement ---
			std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
				return a.height > b.height;
			});

			for (const auto& candidate : candidates) {
				if (forbidden_chunks.count(candidate.chunk)) {
					continue;
				}

				glm::vec3 chunk_pos = glm::vec3(
					candidate.chunk->GetX(),
					candidate.chunk->GetY(),
					candidate.chunk->GetZ()
				);
				glm::vec3 world_pos = chunk_pos + candidate.point;
				auto [terrain_h, terrain_normal] = vis->GetTerrainPointProperties(world_pos.x, world_pos.z);

				if (terrain_h >= 40) {
					// Spawn it
					glm::quat base_rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
					glm::vec3 up_vector = glm::vec3(0.0f, 1.0f, 0.0f);
					glm::quat terrain_alignment = glm::rotation(up_vector, terrain_normal);
					glm::quat final_orientation = terrain_alignment * base_rotation;

					int id = chunk_pos.x + 10 * chunk_pos.y + 100 * chunk_pos.z;
					QueueAddEntity<GuidedMissileLauncher>(
						id,
						Vector3(world_pos.x, world_pos.y, world_pos.z),
						final_orientation
					);
					spawned_launchers_[candidate.chunk] = id;

					// Forbid spawning in neighbors for the rest of this frame
					auto neighbors = get_neighbors(candidate.chunk, visible_chunks);
					forbidden_chunks.insert(neighbors.begin(), neighbors.end());
					forbidden_chunks.insert(candidate.chunk);
				}
			}

			// Despawn old launchers
			for (auto it = spawned_launchers_.begin(); it != spawned_launchers_.end(); /* no increment */) {
				if (visible_chunk_set.find(it->first) == visible_chunk_set.end()) {
					QueueRemoveEntity(it->second);
					it = spawned_launchers_.erase(it);
				} else {
					++it;
				}
			}
		}

		// --- Missile Spawning Logic ---
		auto targets = GetEntitiesByType<PaperPlane>();
		if (targets.empty())
			return;

		auto plane = std::static_pointer_cast<PaperPlane>(targets[0]);
		if (plane && plane->IsDamagePending()) {
			plane->AcknowledgeDamage();
			vis->UpdateHudGauge(3, {3, plane->GetHealth(), "Health", HudAlignment::BOTTOM_CENTER, {0, -50}, {200, 20}});

			auto new_time = damage_dist_(eng_);

			if (damage_timer_ <= 0.0f) { // Only trigger if not already active
				vis->TogglePostProcessingEffect("Glitch", true);
				vis->TogglePostProcessingEffect("Time Stutter", true);
			}

			damage_timer_ = damage_timer_ + new_time;
		}

		damage_timer_ = std::min(damage_timer_, 5.0f);

		/*
		        auto  ppos = plane->GetPosition();
		        float max_h = vis->GetTerrainMaxHeight();

		        float start_h = 0.0f;
		        float extreme_h = 0.0f;

		        // If terrain is not loaded, use a fallback height.
		        if (max_h <= 0.0f) {
		            start_h = 50.0f; // Start firing when plane is reasonably high
		            extreme_h = 200.0f;
		        } else {
		            start_h = (2.0f / 3.0f) * max_h;
		            extreme_h = 3.0f * max_h;
		        }

		        if (ppos.y < start_h)
		            return;
		        const float p_min = 0.5f;  // Missiles per second at start_h
		        const float p_max = 10.0f; // Missiles per second at extreme_h

		        float norm_alt = (ppos.y - start_h) / (extreme_h - start_h);
		        norm_alt = std::min(std::max(norm_alt, 0.0f), 1.0f); // clamp

		        float missiles_per_second = p_min * pow((p_max / p_min), norm_alt);
		        float fire_probability_this_frame = missiles_per_second * delta_time;

		        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
		        if (dist(eng_) < fire_probability_this_frame) {
		            // --- Calculate Firing Location ---
		            // We want to fire from a "rainbow" arc on the terrain that is visible to the camera.

		            // 1. Get camera properties
		            const Camera& camera = vis->GetCamera();
		            glm::vec3     cam_pos = glm::vec3(camera.x, camera.y, camera.z);

		            // This calculation ensures we get the camera's actual forward direction,
		            // even in chase cam mode.
		            glm::vec3 plane_pos_glm = glm::vec3(ppos.x, ppos.y, ppos.z);
		            glm::vec3 cam_fwd = glm::normalize(plane_pos_glm - cam_pos);
		            glm::vec3 cam_right = glm::normalize(glm::cross(cam_fwd, glm::vec3(0.0f, 1.0f, 0.0f)));

		            // 2. Define spawn arc parameters
		            const float kMinSpawnDist = 250.0f;
		            const float kMaxSpawnDist = 400.0f;
		            const float kSpawnFov = glm::radians(camera.fov * 0.9f); // Just under camera FOV

		            // 3. Generate random point in the arc
		            std::uniform_real_distribution<float> dist_dist(kMinSpawnDist, kMaxSpawnDist);
		            std::uniform_real_distribution<float> dist_angle(-kSpawnFov / 2.0f, kSpawnFov / 2.0f);

		            float     rand_dist = dist_dist(eng_);
		            float     rand_angle = dist_angle(eng_);
		            glm::vec3 rand_dir = glm::angleAxis(rand_angle, glm::vec3(0.0f, 1.0f, 0.0f)) * cam_fwd;

		            // 4. Find the point on the terrain
		            glm::vec3 ray_origin = cam_pos;
		            // We push the origin forward a bit to ensure the spawn is always in front and far away
		            ray_origin += rand_dir * rand_dist;

		            float terrain_h = 0.0f;
		            if (max_h > 0.0f) {
		                std::tuple<float, glm::vec3> props = vis->GetTerrainPointProperties(ray_origin.x, ray_origin.z);
		                terrain_h = std::get<0>(props);

		                // Safety check: ensure missile doesn't spawn underground or too high if terrain is weird
		                if (terrain_h < 0.0f || !std::isfinite(terrain_h)) {
		                    return;
		                }
		            }

		            Vector3 launchPos = Vector3(ray_origin.x, terrain_h, ray_origin.z);

		            QueueAddEntity<GuidedMissile>(launchPos);
		        }
		*/
	}

private:
	std::map<const Terrain*, int>         spawned_launchers_;
	std::random_device                    rd_;
	std::mt19937                          eng_;
	float                                 damage_timer_ = 0.0f;
	std::uniform_real_distribution<float> damage_dist_;
};

int main() {
	try {
		auto visualizer = std::make_shared<Visualizer>(1280, 720, "Terrain Demo");
		visualizer->AddHudIcon(
			{1, "assets/missile-icon.png", HudAlignment::TOP_LEFT, {10, 10}, {64, 64}, selected_weapon == 0}
		);
		visualizer->AddHudIcon(
			{2, "assets/bomb-icon.png", HudAlignment::TOP_LEFT, {84, 10}, {64, 64}, selected_weapon == 1}
		);

		auto [height, norm] = visualizer->GetTerrainPointProperties(0, 0);

		auto handler = PaperPlaneHandler(visualizer->GetThreadPool());
		handler.SetVisualizer(visualizer);
		auto id = handler.AddEntity<PaperPlane>();
		auto plane = handler.GetEntity(id);
		plane->SetPosition(0, height + 10, 0);
		Boidsish::Camera camera(0.0f, height + 15, -10.0f);
		visualizer->SetCamera(camera);

		visualizer->AddShapeHandler(std::ref(handler));
		visualizer->SetChaseCamera(plane);

		visualizer->AddHudGauge({3, 100.0f, "Health", HudAlignment::BOTTOM_CENTER, {0, -50}, {200, 20}});

		auto controller = std::make_shared<PaperPlaneInputController>();
		std::dynamic_pointer_cast<PaperPlane>(plane)->SetController(controller);

		visualizer->AddInputCallback([&](const Boidsish::InputState& state) {
			controller->pitch_up = state.keys[GLFW_KEY_S];
			controller->pitch_down = state.keys[GLFW_KEY_W];
			controller->yaw_left = state.keys[GLFW_KEY_A];
			controller->yaw_right = state.keys[GLFW_KEY_D];
			controller->roll_left = state.keys[GLFW_KEY_Q];
			controller->roll_right = state.keys[GLFW_KEY_E];
			controller->boost = state.keys[GLFW_KEY_LEFT_SHIFT];
			controller->brake = state.keys[GLFW_KEY_LEFT_CONTROL];
			controller->fire = state.keys[GLFW_KEY_SPACE];
			if (state.key_down[GLFW_KEY_F]) {
				selected_weapon = (selected_weapon + 1) % 2;
				visualizer->UpdateHudIcon(
					1,
					{1, "assets/missile-icon.png", HudAlignment::TOP_LEFT, {10, 10}, {64, 64}, selected_weapon == 0}
				);
				visualizer->UpdateHudIcon(
					2,
					{2, "assets/bomb-icon.png", HudAlignment::TOP_LEFT, {84, 10}, {64, 64}, selected_weapon == 1}
				);
			}
		});

		visualizer->Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
