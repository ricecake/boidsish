#include "walking_tank.h"
#include "procedural_mesher.h"
#include <numbers>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

	WalkingTank::WalkingTank(int id, float x, float y, float z) :
		Entity<Model>(id) {

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
		ProceduralIR ir;
		ir.name = "tank_mesh";

		float hip_h = 1.0f;
		float knee_h = 0.5f;
		float foot_h = 0.0f;
		float body_h = 0.9f;

		// Body Hub
		int body_hub = ir.AddHub(glm::vec3(0, body_h, 0), 0.8f, glm::vec3(0.4f, 0.45f, 0.4f));
		// Turret
		ir.AddHub(glm::vec3(0, body_h + 0.4f, 0.2f), 0.5f, glm::vec3(0.35f, 0.4f, 0.35f), body_hub);

		// 4 Legs
		float w = 0.8f;
		float l = 0.8f;

		std::vector<glm::vec3> offsets = {
			{-w, hip_h,  l}, { w, hip_h,  l},
			{-w, hip_h, -l}, { w, hip_h, -l}
		};

		for(int i = 0; i < 4; ++i) {
			glm::vec3 base_pos = offsets[i];
			glm::vec3 knee_pos = base_pos + glm::vec3(0, knee_h - hip_h, 0.3f);
			glm::vec3 foot_pos = knee_pos + glm::vec3(0, foot_h - knee_h, -0.3f);

			// Leg geometry
			int hip = ir.AddHub(base_pos, 0.2f, glm::vec3(0.3f), body_hub);
			int u_leg = ir.AddTube(base_pos, knee_pos, 0.12f, 0.1f, glm::vec3(0.35f), hip);
			int knee = ir.AddHub(knee_pos, 0.15f, glm::vec3(0.3f), u_leg);
			int l_leg = ir.AddTube(knee_pos, foot_pos, 0.1f, 0.08f, glm::vec3(0.35f), knee);
			ir.AddHub(foot_pos, 0.12f, glm::vec3(0.2f), l_leg);
		}

		shape_ = ProceduralMesher::GenerateModel(ir);

		// Build skeleton manually
		shape_->AddBone("body", "", glm::translate(glm::mat4(1.0f), glm::vec3(0, body_h, 0)));

		legs_.clear();
		for(int i = 0; i < 4; ++i) {
			std::string leg_prefix = "leg_" + std::to_string(i);
			glm::vec3 base_pos = offsets[i];
			glm::vec3 knee_pos = base_pos + glm::vec3(0, knee_h - hip_h, 0.3f);
			glm::vec3 foot_pos = knee_pos + glm::vec3(0, foot_h - knee_h, -0.3f);

			glm::vec3 hip_rel = base_pos - glm::vec3(0, body_h, 0);
			shape_->AddBone(leg_prefix + "_hip", "body", glm::translate(glm::mat4(1.0f), hip_rel));

			glm::vec3 knee_rel = knee_pos - base_pos;
			shape_->AddBone(leg_prefix + "_knee", leg_prefix + "_hip", glm::translate(glm::mat4(1.0f), knee_rel));

			glm::vec3 foot_rel = foot_pos - knee_pos;
			shape_->AddBone(leg_prefix + "_foot", leg_prefix + "_knee", glm::translate(glm::mat4(1.0f), foot_rel));

			Leg leg;
			leg.name = leg_prefix;
			leg.effector = leg_prefix + "_foot";
			leg.rest_offset = foot_pos;
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

		// Ground clamping
		auto [terrain_h, normal] = handler.GetTerrainPropertiesAtPoint(current_pos.x, current_pos.z);
		current_pos.y = terrain_h;
		rigid_body_.SetPosition(current_pos);

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
			current_pos += glm::vec3(std::sin(glm::radians(current_yaw_)), 0, std::cos(glm::radians(current_yaw_))) * speed * delta_time;
			rigid_body_.SetPosition(current_pos);
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
				legs_[i].world_foot_pos.y = rigid_body_.GetPosition().y + std::sin(t * std::numbers::pi_v<float>) * step_height_;
			} else {
				if (legs_[i].is_moving) {
					legs_[i].is_moving = false;
					legs_[i].world_foot_pos = legs_[i].step_target_pos;
					legs_[i].world_foot_pos.y = rigid_body_.GetPosition().y;
				}

				// Keep foot planted relative to ground if body moves
				// In a more complex sim, we'd clamp to terrain
			}

			// Solve IK for the leg
			shape_->SolveIK(legs_[i].effector, legs_[i].world_foot_pos, 0.01f, 10, "body");
		}
	}

} // namespace Boidsish
