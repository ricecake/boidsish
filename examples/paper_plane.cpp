#include <iostream>
#include <memory>
#include <random>
#include <vector>

#include "arrow.h"
#include "dot.h"
#include "field.h"
#include "graphics.h"
#include "logger.h"
#include "model.h"
#include "spatial_entity_handler.h"
#include "terrain_generator.h"
#include <GLFW/glfw3.h>
#include <fire_effect.h>
#include <glm/gtc/quaternion.hpp>

using namespace Boidsish;

class CatMissile;

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

		const float kBaseSpeed = 30.0f;
		const float kBoostSpeed = 80.0f;
		const float kBreakSpeed = 10.0f;
		const float kBoostAcceleration = 120.0f;
		const float kSpeedDecay = 10.0f;

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
			// This robust auto-leveling logic finds the shortest rotational path to bring the plane upright and level.
			// It works by finding where the world's 'up' vector is in relation to the plane's local axes,
			// and then applying corrective forces.

			glm::vec3 world_up_in_local = glm::inverse(orientation_) * glm::vec3(0.0f, 1.0f, 0.0f);

			// --- Pitch Correction (Shortest Path) ---
			// The 'z' component of world_up_in_local tells us how 'forward' or 'backward' the world's 'up' is.
			// A positive 'z' means world 'up' is in front of our nose (i.e., we are pitched down).
			// To correct, we pitch up (positive x rotation), correctly taking the shortest path to the horizon.
			target_rot_velocity.x += world_up_in_local.z * kAutoLevelSpeed;

			// --- Roll Correction ---
			// The 'x' component tells us how 'right' or 'left' the world's 'up' is.
			// A positive 'x' means world 'up' is to our right. We must roll right (negative z rotation) to level the
			// wings.
			float roll_correction = world_up_in_local.x * kAutoLevelSpeed;

			// The 'y' component tells us if we are upright or inverted. If it's negative, we are upside down.
			if (world_up_in_local.y < 0.0f) {
				roll_correction *= 3.0f; // Apply a stronger roll correction.
				target_rot_velocity.x = 0;

				// This solves the "stuck upside down" problem. If we're perfectly inverted, the roll_correction
				// can be zero. Here, we add a constant roll "kick" to get the plane rolling.
				if (abs(world_up_in_local.x) < 0.1f) {
					roll_correction += kRollSpeed * 0.5f;
				}
			}

			target_rot_velocity.z -= roll_correction;
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
			handler.QueueAddEntity<CatMissile>(
				GetPosition(),
				orientation_,
				orientation_ * glm::vec3(fire_left ? -1 : 1, -1, 0),
				GetVelocity()
			);
			time_to_fire = 0.25f;
			fire_left = !fire_left;
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

	void TriggerDamage() { damage_pending_ = true; }

	bool IsDamagePending() { return damage_pending_; }

	void AcknowledgeDamage() { damage_pending_ = false; }

private:
	std::shared_ptr<PaperPlaneInputController> controller_;
	glm::quat                                  orientation_;
	glm::vec3                                  rotational_velocity_; // x: pitch, y: yaw, z: roll
	float                                      forward_speed_;
	float                                      time_to_fire = 0.25f;
	bool                                       fire_left = true;
	bool                                       damage_pending_;
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
		lived += delta_time;
		if (lived >= lifetime) {
			if (exploded) {
				// handler.EnqueueVisualizerAction([&]() {
				// 	fire->Remove();
				// });
				handler.QueueRemoveEntity(id_);
				return;
			}
			lived = -2;
			exploded = true;
			handler.EnqueueVisualizerAction([&]() { fire->SetStyle(2); });
		}
		if (exploded) {
			return;
		}
		auto pos = GetPosition();
		if (fire == nullptr) {
			handler.EnqueueVisualizerAction([&]() {
				fire = handler.vis->AddFireEffect(glm::vec3(pos.x, pos.y, pos.z), orientation_ * glm::vec3(0, 0, -1));
				fire->SetStyle(1);
			});
		} else {
			handler.EnqueueVisualizerAction([&]() {
				fire->SetPosition({pos.x, pos.y, pos.z});
				fire->SetDirection(orientation_ * glm::vec3(0, 0, -1));
			});
		}

		// --- Flight Model Constants ---
		const float kLaunchTime = 0.5f;
		const float kMaxSpeed = 150.0f;
		const float kAcceleration = 150.0f;

		// --- Launch Phase ---
		if (lived < kLaunchTime) {
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
					SetVelocity(0, 0, 0);
					SetSize(100);
					SetColor(1, 0, 0, 0.33f);
					exploded = true;
					lived = -5; // Used for explosion lifetime
					handler.EnqueueVisualizerAction([&]() { fire->SetStyle(2); });
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

				if (lived <= 1.5f) {
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

private:
	constexpr static int        thrust{50};
	constexpr static int        lifetime{12};
	float                       lived = 0;
	bool                        exploded = false;
	bool                        fired = false;
	std::shared_ptr<FireEffect> fire;

	// Flight model
	glm::quat          orientation_;
	glm::vec3          rotational_velocity_; // x: pitch, y: yaw, z: roll
	float              forward_speed_;
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
		lived += delta_time;
		if (lived >= lifetime) {
			if (exploded) {
				// handler.EnqueueVisualizerAction([&]() {
				// 	fire->Remove();
				// });
				handler.QueueRemoveEntity(id_);
				return;
			}
			lived = -2;
			exploded = true;
			handler.EnqueueVisualizerAction([&]() { fire->SetStyle(2); });
		}
		if (exploded) {
			return;
		}

		// --- Flight Model Constants ---
		const float kLaunchTime = 1.0f;
		const float kMaxSpeed = 150.0f;
		const float kAcceleration = 150.0f;

		// --- Launch Phase ---
		if (lived < kLaunchTime) {
			auto velo = GetVelocity();
			velo += Vector3(0, -0.07f, 0);
			SetVelocity(velo);
			return;
		} else {
			auto pos = GetPosition();
			if (!fired) {
				SetTrailLength(500);
				SetTrailRocket(true);
				SetOrientToVelocity(true);

				fired = true;
				handler.EnqueueVisualizerAction([&]() {
					fire = handler.vis->AddFireEffect(
						glm::vec3(pos.x, pos.y, pos.z),
						orientation_ * glm::vec3(0, 0, -1)
					);
					fire->SetStyle(1);
				});
			} else {
				handler.EnqueueVisualizerAction([&]() {
					fire->SetPosition({pos.x, pos.y, pos.z});
					fire->SetDirection(orientation_ * glm::vec3(0, 0, -1));
				});
			}

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
					SetVelocity(0, 0, 0);
					SetSize(100);
					SetColor(1, 0, 0, 0.33f);
					exploded = true;
					lived = -5; // Used for explosion lifetime
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
			if (lived <= 1.5f) {
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

private:
	constexpr static int        thrust{50};
	constexpr static int        lifetime{12};
	float                       lived = 0;
	bool                        exploded = false;
	bool                        fired = false;
	std::shared_ptr<FireEffect> fire;

	// Flight model
	glm::quat          orientation_;
	glm::vec3          rotational_velocity_; // x: pitch, y: yaw, z: roll
	float              forward_speed_;
	std::random_device rd_;
	std::mt19937       eng_;
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
				vis->TogglePostProcessingEffect("Glitch");
				vis->TogglePostProcessingEffect("TimeStutter");
			}
		}

		// --- Missile Spawning Logic ---
		auto targets = GetEntitiesByType<PaperPlane>();
		if (targets.empty())
			return;

		auto plane = std::static_pointer_cast<PaperPlane>(targets[0]);
		if (plane && plane->IsDamagePending()) {
			plane->AcknowledgeDamage();
			if (damage_timer_ <= 0.0f) { // Only trigger if not already active
				damage_timer_ = damage_dist_(eng_);
				vis->TogglePostProcessingEffect("Glitch");
				vis->TogglePostProcessingEffect("TimeStutter");
			}
		}

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
			auto    fire_effect = vis->AddFireEffect(glm::vec3(0.0f, 3.0f, 0.0f), glm::vec3(0, 1, 0));

			AddEntity<GuidedMissile>(launchPos);
		}
	}

private:
	std::random_device                    rd_;
	std::mt19937                          eng_;
	float                                 damage_timer_ = 0.0f;
	std::uniform_real_distribution<float> damage_dist_;
};

int main() {
	try {
		auto visualizer = std::make_shared<Visualizer>(1280, 720, "Terrain Demo");

		Boidsish::Camera camera;
		visualizer->SetCamera(camera);
		auto [height, norm] = visualizer->GetTerrainPointProperties(0, 0);

		auto handler = PaperPlaneHandler(visualizer->GetThreadPool());
		handler.SetVisualizer(visualizer);
		auto id = handler.AddEntity<PaperPlane>();
		auto plane = handler.GetEntity(id);
		plane->SetPosition(0, height + 10, 0);

		visualizer->AddShapeHandler(std::ref(handler));
		visualizer->SetChaseCamera(plane);

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
		});

		visualizer->Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
