#include "walking_tank.h"
#include <numbers>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

	WalkingTank::WalkingTank(int id, float x, float y, float z) :
		Entity<Model>(id, std::make_shared<ModelData>()) {

		rigid_body_.SetPosition(glm::vec3(x, y, z));
		target_pos_ = glm::vec3(x, y, z);

		InitializeRig();

		// Set methodical tank-like properties
		SetSize(2.5f);
		SetColor(0.4f, 0.45f, 0.4f); // Military green
		SetUsePBR(true);
		SetRoughness(0.8f);
		SetMetallic(0.2f);
	}

	void WalkingTank::InitializeRig() {
		auto data = shape_->GetData();

		// Build a simple robotic rig
		// Body
		data->AddBone("body", "", glm::mat4(1.0f));

		// 4 Legs
		float w = 1.0f;
		float l = 1.0f;
		float h = 0.5f;

		std::vector<std::string> leg_names = {"FL", "FR", "BL", "BR"};
		std::vector<glm::vec3> offsets = {
			{-w, -h,  l}, { w, -h,  l},
			{-w, -h, -l}, { w, -h, -l}
		};

		for(int i = 0; i < 4; ++i) {
			std::string base = leg_names[i] + "_base";
			std::string knee = leg_names[i] + "_knee";
			std::string foot = leg_names[i] + "_foot";

			// Robotic legs: pivot out, then down, then down to foot
			data->AddBone(base, "body", glm::translate(glm::mat4(1.0f), offsets[i]));
			data->AddBone(knee, base, glm::translate(glm::mat4(1.0f), glm::vec3(0, -0.5f, 0.2f)));
			data->AddBone(foot, knee, glm::translate(glm::mat4(1.0f), glm::vec3(0, -0.5f, -0.2f)));

			Leg leg;
			leg.name = leg_names[i];
			leg.effector = foot;
			leg.rest_offset = offsets[i] + glm::vec3(0, -1.0f, 0);
			leg.world_foot_pos = rigid_body_.GetPosition() + leg.rest_offset;
			legs_.push_back(leg);
		}

		shape_->SkinToHierarchy();
	}

	void WalkingTank::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		(void)time;
		UpdateMovement(handler, delta_time);
		UpdateShape();
	}

	void WalkingTank::UpdateMovement(const EntityHandler& handler, float delta_time) {
		glm::vec3 current_pos = rigid_body_.GetPosition();
		glm::vec3 diff = target_pos_ - current_pos;
		diff.y = 0;
		float dist = glm::length(diff);

		bool is_walking = dist > 0.1f;

		if (is_walking) {
			glm::vec3 dir = glm::normalize(diff);
			float target_yaw = glm::degrees(std::atan2(dir.x, dir.z));

			float yaw_diff = target_yaw - current_yaw_;
			while (yaw_diff > 180.0f) yaw_diff -= 360.0f;
			while (yaw_diff < -180.0f) yaw_diff += 360.0f;
			current_yaw_ += yaw_diff * std::min(1.0f, delta_time * 2.0f);

			float speed = 1.5f;
			rigid_body_.SetPosition(current_pos + glm::vec3(std::sin(glm::radians(current_yaw_)), 0, std::cos(glm::radians(current_yaw_))) * speed * delta_time);
		}

		rigid_body_.SetOrientation(glm::angleAxis(glm::radians(current_yaw_), glm::vec3(0, 1, 0)));

		if (is_walking) {
			walk_phi_ += (delta_time / step_duration_);
			if (walk_phi_ >= 2.0f) walk_phi_ -= 2.0f;
		}

		glm::vec3 forward = rigid_body_.GetOrientation() * glm::vec3(0, 0, 1);

		for (int i = 0; i < (int)legs_.size(); ++i) {
			// Cross-gait: FL and BR move together, FR and BL move together
			float leg_phi_offset = (i == 0 || i == 3) ? 0.0f : 1.0f;
			float leg_phi = walk_phi_ - leg_phi_offset;
			while (leg_phi < 0) leg_phi += 2.0f;
			while (leg_phi >= 2.0f) leg_phi -= 2.0f;

			if (is_walking && leg_phi < 1.0f) {
				if (!legs_[i].is_moving) {
					legs_[i].is_moving = true;
					legs_[i].step_start_pos = legs_[i].world_foot_pos;

					glm::vec3 rotated_offset = rigid_body_.GetOrientation() * legs_[i].rest_offset;
					legs_[i].step_target_pos = rigid_body_.GetPosition() + rotated_offset + forward * stride_length_;
				}

				float t = leg_phi; // 0 to 1
				// Robotic parabolic step
				legs_[i].world_foot_pos = glm::mix(legs_[i].step_start_pos, legs_[i].step_target_pos, t);
				legs_[i].world_foot_pos.y = rigid_body_.GetPosition().y - 1.0f + std::sin(t * std::numbers::pi_v<float>) * step_height_;
			} else {
				if (legs_[i].is_moving) {
					legs_[i].is_moving = false;
					legs_[i].world_foot_pos = legs_[i].step_target_pos;
					legs_[i].world_foot_pos.y = rigid_body_.GetPosition().y - 1.0f;
				}

				// Keep foot planted relative to ground if body moves
				// In a more complex sim, we'd clamp to terrain
			}

			// Solve IK for the leg
			shape_->SolveIK(legs_[i].effector, legs_[i].world_foot_pos, 0.01f, 10, "body");
		}
	}

} // namespace Boidsish
