#include "JetPlane.h"
#include "entity.h"
#include <glm/gtx/quaternion.hpp>
#include <algorithm>

namespace Boidsish {

	JetPlane::JetPlane(int id) :
		Entity<Model>(id, "assets/Paper Airplane.obj", true),
		orientation_(glm::quat(1.0f, 0.0f, 0.0f, 0.0f)),
		rotational_velocity_(glm::vec3(0.0f)),
		forward_speed_(30.0f),
		throttle_(0.5f) {

		rigid_body_.linear_friction_ = 0.01f;
		rigid_body_.angular_friction_ = 0.01f;

		SetTrailLength(20);
		SetTrailIridescence(true);
		SetColor(0.1f, 0.5f, 1.0f);
		shape_->SetScale(glm::vec3(0.05f));

		SetPosition(0, 100, 0);
		SetVelocity(Vector3(0, 0, -forward_speed_));

		UpdateShape();
	}

	void JetPlane::SetController(std::shared_ptr<JetInputController> controller) {
		controller_ = controller;
	}

	void JetPlane::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		(void)handler;
		(void)time;
		if (!controller_)
			return;

		// Handle throttle
		if (controller_->throttle_up) {
			throttle_ += 0.5f * delta_time;
		}
		if (controller_->throttle_down) {
			throttle_ -= 0.5f * delta_time;
		}
		throttle_ = std::clamp(throttle_, 0.0f, 1.0f);

		if (controller_->toggle_stabilization) {
			stabilization_enabled_ = !stabilization_enabled_;
			controller_->toggle_stabilization = false; // Reset toggle
		}

		// Speed range
		const float kMinSpeed = 10.0f;
		const float kMaxSpeed = 300.0f;
		const float kAcceleration = 50.0f;

		float target_speed = kMinSpeed + (kMaxSpeed - kMinSpeed) * throttle_;

		if (forward_speed_ < target_speed) {
			forward_speed_ += kAcceleration * delta_time;
			if (forward_speed_ > target_speed)
				forward_speed_ = target_speed;
		} else if (forward_speed_ > target_speed) {
			forward_speed_ -= kAcceleration * delta_time;
			if (forward_speed_ < target_speed)
				forward_speed_ = target_speed;
		}

		// Handle rotation inputs
		// Controls should become touchy at lower speeds, and stiff at high speeds.
		// Higher speed = lower sensitivity factor
		float sensitivity_factor = 20.0f / (forward_speed_ + 5.0f);
		sensitivity_factor = std::clamp(sensitivity_factor, 0.2f, 2.0f);

		// Adjust sensitivity based on control and direction
		// Pitch up and roll should be much less affected by damping
		float pitch_up_sensitivity = std::max(sensitivity_factor, 0.8f);
		float roll_sensitivity = std::max(sensitivity_factor, 0.7f);
		// Yaw should be less affected
		float yaw_sensitivity = std::max(sensitivity_factor, 0.5f);
		// Pitch down is fully affected (encourages rolling and pulling up)
		float pitch_down_sensitivity = sensitivity_factor;

		const float kBasePitchSpeed = 1.5f;
		const float kBaseYawSpeed = 0.4f; // Sluggish yaw
		const float kBaseRollSpeed = 3.0f;
		const float kAutoLevelSpeed = 1.5f;
		const float kDamping = 2.5f;

		glm::vec3 target_rot_velocity = glm::vec3(0.0f);
		if (controller_->pitch_up)
			target_rot_velocity.x += kBasePitchSpeed * pitch_up_sensitivity;
		if (controller_->pitch_down)
			target_rot_velocity.x -= kBasePitchSpeed * pitch_down_sensitivity;
		if (controller_->yaw_left)
			target_rot_velocity.y += kBaseYawSpeed * yaw_sensitivity;
		if (controller_->yaw_right)
			target_rot_velocity.y -= kBaseYawSpeed * yaw_sensitivity;
		if (controller_->roll_left)
			target_rot_velocity.z += kBaseRollSpeed * roll_sensitivity;
		if (controller_->roll_right)
			target_rot_velocity.z -= kBaseRollSpeed * roll_sensitivity;

		// Coordinated Turn (Banking)
		target_rot_velocity.z += target_rot_velocity.y * 0.8f;

		// Auto-leveling (stabilization)
		if (stabilization_enabled_ && !controller_->pitch_up && !controller_->pitch_down &&
		    !controller_->yaw_left && !controller_->yaw_right && !controller_->roll_left &&
		    !controller_->roll_right) {
			glm::vec3 world_up = glm::vec3(0.0f, 1.0f, 0.0f);
			glm::vec3 plane_forward_world = orientation_ * glm::vec3(0.0f, 0.0f, -1.0f);
			glm::vec3 world_up_in_local = glm::inverse(orientation_) * world_up;

			float pitch_error = glm::asin(glm::dot(plane_forward_world, world_up));
			float roll_error = atan2(world_up_in_local.x, world_up_in_local.y);

			if (abs(glm::dot(plane_forward_world, world_up)) > 0.99f) {
				roll_error = 0.0f;
			}

			target_rot_velocity.x -= pitch_error * kAutoLevelSpeed;
			target_rot_velocity.z -= roll_error * kAutoLevelSpeed;
		}

		rotational_velocity_ += (target_rot_velocity - rotational_velocity_) * kDamping * delta_time;

		glm::quat pitch_delta = glm::angleAxis(rotational_velocity_.x * delta_time, glm::vec3(1.0f, 0.0f, 0.0f));
		glm::quat yaw_delta = glm::angleAxis(rotational_velocity_.y * delta_time, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::quat roll_delta = glm::angleAxis(rotational_velocity_.z * delta_time, glm::vec3(0.0f, 0.0f, 1.0f));
		orientation_ = glm::normalize(orientation_ * pitch_delta * yaw_delta * roll_delta);
		rigid_body_.SetOrientation(orientation_);

		glm::vec3 forward_dir = orientation_ * glm::vec3(0.0f, 0.0f, -1.0f);
		glm::vec3 new_velocity = forward_dir * forward_speed_;
		SetVelocity(Vector3(new_velocity.x, new_velocity.y, new_velocity.z));
	}

	void JetPlane::UpdateShape() {
		Entity<Model>::UpdateShape();
		if (shape_) {
			shape_->SetRotation(orientation_);
		}
	}

} // namespace Boidsish
