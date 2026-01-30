#include "PaperPlane.h"

#include "CatBomb.h"
#include "CatMissile.h"
#include "PaperPlaneHandler.h" // For selected_weapon
#include "entity.h"
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

	PaperPlane::PaperPlane(int id):
		Entity<Model>(id, "assets/Mesh_Cat.obj", true),
		orientation_(glm::quat(1.0f, 0.0f, 0.0f, 0.0f)),
		rotational_velocity_(glm::vec3(0.0f)),
		forward_speed_(20.0f) {
		rigid_body_.linear_friction_ = 0.01f;
		rigid_body_.angular_friction_ = 0.01f;

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

	void PaperPlane::SetController(std::shared_ptr<PaperPlaneInputController> controller) {
		controller_ = controller;
	}

	void PaperPlane::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		if (!controller_)
			return;

		// --- Constants for flight model ---
		const float kPitchSpeed = 1.5f;
		const float kYawSpeed = 1.5f;
		const float kRollSpeed = 3.0f;
		const float kCoordinatedTurnFactor = 0.8f;
		const float kAutoLevelSpeed = 1.5f;
		const float kDamping = 2.5f;

		const float kBaseSpeed = 50.0f;
		const float kBoostSpeed = 80.0f;
		const float kBreakSpeed = 10.0f;
		const float kBoostAcceleration = 100.0f;
		const float kSpeedDecay = 30.0f;

		auto pos = GetPosition();
		auto [height, norm] = handler.vis->GetTerrainPointPropertiesThreadSafe(pos.x, pos.z);
		if (pos.y < height) {
			TriggerDamage();
			// pos = height;
			auto newPos = glm::vec3{pos.x, height, pos.z} + norm * 0.1f;
			SetPosition(newPos);
			glm::vec3 forward_dir = orientation_ * glm::vec3(0.0f, 0.0f, -1.0f);
			auto      new_forward = glm::reflect(forward_dir, norm);
			orientation_ = glm::lookAt(pos.Toglm(), pos.Toglm() + new_forward, glm::vec3(0, 1, 0));
			forward_dir = orientation_ * glm::vec3(0.0f, 0.0f, -1.0f);
			glm::vec3 new_velocity = forward_dir * forward_speed_ * 0.150f;
			SetVelocity(Vector3(new_velocity.x, new_velocity.y, new_velocity.z));

			return;
		}

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
		target_rot_velocity.z += target_rot_velocity.y * kCoordinatedTurnFactor;

		// --- Auto-leveling ---
		if (!controller_->pitch_up && !controller_->pitch_down && !controller_->yaw_left && !controller_->yaw_right &&
		    !controller_->roll_left && !controller_->roll_right) {
			glm::vec3 world_up = glm::vec3(0.0f, 1.0f, 0.0f);
			glm::vec3 plane_forward_world = orientation_ * glm::vec3(0.0f, 0.0f, -1.0f);
			glm::vec3 world_up_in_local = glm::inverse(orientation_) * world_up;

			glm::vec3 forward_on_horizon = glm::normalize(
				glm::vec3(plane_forward_world.x, 0.0f, plane_forward_world.z)
			);
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

		glm::vec3 forward_dir = orientation_ * glm::vec3(0.0f, 0.0f, -1.0f);
		glm::vec3 new_velocity = forward_dir * forward_speed_;
		SetVelocity(Vector3(new_velocity.x, new_velocity.y, new_velocity.z));

		time_to_fire -= delta_time;
		if (controller_->fire && time_to_fire <= 0) {
			switch (selected_weapon) {
			case 0:
				handler.QueueAddEntity<CatMissile>(
					pos,
					orientation_,
					glm::normalize(glm::vec3(fire_left ? -1.0f : 1.0f, -1.0f, 0.0f)),
					GetVelocity()
				);
				time_to_fire = 0.25f;
				fire_left = !fire_left;
				if (fire_left) {
					time_to_fire = 1.0f;
				}
				break;
			case 1:
				handler.QueueAddEntity<CatBomb>(pos, orientation_ * glm::vec3(0, -1, 0), GetVelocity());
				time_to_fire = 1.25f;
				break;
			}
		}

		if (true || controller_->chaff) {
			// handler.vis->AddFireEffect(
			// 	pos.Toglm() - forward_dir,
			// 	FireEffectStyle::Glitter,
			// 	glm::normalize(-1*forward_dir),
			// 	glm::vec3(0, 0, 0),
			// 	500,
			// 	3.0f
			// );
			handler.vis->AddFireEffect(
				glm::vec3(0, 5, 0),
				FireEffectStyle::Glitter,
				glm::vec3(0, 1, 0),
				glm::vec3(0, 0, 0),
				500,
				3.0f
			);
		}
	}

	void PaperPlane::UpdateShape() {
		Entity<Model>::UpdateShape();
		if (shape_) {
			shape_->SetRotation(orientation_);
		}
	}

	void PaperPlane::TriggerDamage() {
		health -= 5;
		damage_pending_++;
	}

	bool PaperPlane::IsDamagePending() {
		return damage_pending_ > 0;
	}

	void PaperPlane::AcknowledgeDamage() {
		damage_pending_--;
	}

	float PaperPlane::GetHealth() const {
		return health;
	}

	float PaperPlane::GetShield() const {
		return shield;
	}

} // namespace Boidsish
