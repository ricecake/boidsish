#include <iostream>
#include <memory>
#include <vector>

#include "arrow.h"
#include "dot.h"
#include "field.h"
#include "graphics.h"
#include "model.h"
#include "spatial_entity_handler.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/quaternion.hpp>

using namespace Boidsish;

struct PaperPlaneInputController {
	bool pitch_up = false;
	bool pitch_down = false;
	bool yaw_left = false;
	bool yaw_right = false;
	bool roll_left = false;
	bool roll_right = false;
	bool boost = false;
};

class PaperPlane: public Entity<Model> {
public:
	PaperPlane(int id = 0):
		Entity<Model>(id, "assets/Paper Airplane.obj", true),
		orientation_(glm::quat(1.0f, 0.0f, 0.0f, 0.0f)),
		rotational_velocity_(glm::vec3(0.0f)),
		forward_speed_(20.0f) {
		// All entities need to have their shape updated at least once.
		UpdateShape();
		SetTrailLength(100);
		SetTrailIridescence(true);

		SetColor(1.0f, 0.5f, 0.0f);
		shape_->SetScale(glm::vec3(0.01f));
		SetPosition(0, 4, 0);

		// Initial velocity for a nice takeoff
		SetVelocity(Vector3(0, 0, 20));

		// Correct the initial orientation to match the model's alignment
		orientation_ = glm::angleAxis(glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
			glm::angleAxis(glm::radians(25.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	}

	void SetController(std::shared_ptr<PaperPlaneInputController> controller) { controller_ = controller; }

	void UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		if (!controller_)
			return;

		// --- Constants for flight model ---
		const float kPitchSpeed = 2.0f;
		const float kYawSpeed = 1.5f;
		const float kRollSpeed = 3.0f;
		const float kCoordinatedTurnFactor = 0.8f;
		const float kAutoLevelSpeed = 1.5f;
		const float kDamping = 2.5f;

		const float kBaseSpeed = 20.0f;
		const float kBoostSpeed = 80.0f;
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
		// If there's no user input, gently guide the plane to a stable, level orientation.
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
			// A positive 'x' means world 'up' is to our right. We must roll right (negative z rotation) to level the wings.
			float roll_correction = world_up_in_local.x * kAutoLevelSpeed;

			// The 'y' component tells us if we are upright or inverted. If it's negative, we are upside down.
			if (world_up_in_local.y < 0.0f) {
				// This fulfills the user's request to "prefer a roll" when inverted.
				roll_correction *= 2.0f; // Apply a stronger roll correction.

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
		} else {
			if (forward_speed_ > kBaseSpeed) {
				forward_speed_ -= kSpeedDecay * delta_time;
				if (forward_speed_ < kBaseSpeed)
					forward_speed_ = kBaseSpeed;
			}
		}

		// --- Update Velocity and Position ---
		// The model's "forward" is along the negative Z-axis in its local space
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
	std::shared_ptr<PaperPlaneInputController> controller_;
	glm::quat                                  orientation_;
	glm::vec3                                  rotational_velocity_; // x: pitch, y: yaw, z: roll
	float                                      forward_speed_;
};

class PaperPlaneHandler: public SpatialEntityHandler {
public:
	PaperPlaneHandler(task_thread_pool::task_thread_pool& thread_pool): SpatialEntityHandler(thread_pool) {}
};

int main() {
	try {
		Boidsish::Visualizer visualizer(1280, 720, "Terrain Demo");

		Boidsish::Camera camera;
		visualizer.SetCamera(camera);

		auto handler = PaperPlaneHandler(visualizer.GetThreadPool());
		auto id = handler.AddEntity<PaperPlane>();
		auto plane = handler.GetEntity(id);

		visualizer.AddShapeHandler(std::ref(handler));
		visualizer.SetChaseCamera(plane);

		auto controller = std::make_shared<PaperPlaneInputController>();
		std::dynamic_pointer_cast<PaperPlane>(plane)->SetController(controller);

		visualizer.AddInputCallback([&](const Boidsish::InputState& state) {
			controller->pitch_up = state.keys[GLFW_KEY_S];
			controller->pitch_down = state.keys[GLFW_KEY_W];
			controller->yaw_left = state.keys[GLFW_KEY_A];
			controller->yaw_right = state.keys[GLFW_KEY_D];
			controller->roll_left = state.keys[GLFW_KEY_Q];
			controller->roll_right = state.keys[GLFW_KEY_E];
			controller->boost = state.keys[GLFW_KEY_SPACE];
		});

		visualizer.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
