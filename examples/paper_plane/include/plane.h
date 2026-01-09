#pragma once

#include <iostream>
#include <memory>
#include <random>
#include <set>
#include <vector>

#include "bomb.h"
#include "dot.h"
#include "cat_missile.h"
#include "emplacements.h"
#include "graphics.h"
#include "guided_missile.h"
#include "handler.h"
#include "hud.h"
#include "logger.h"
#include "model.h"
#include "spatial_entity_handler.h"
#include "terrain_generator.h"
#include <fire_effect.h>

using namespace Boidsish;

using namespace Boidsish;

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
	int selected_weapon = 0;
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
			switch (controller_->selected_weapon) {
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
