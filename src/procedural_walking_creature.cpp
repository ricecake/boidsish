#include "procedural_walking_creature.h"
#include "service_locator.h"
#include "graphics.h"
#include "terrain_generator.h"


#include <algorithm>
#include <cmath>
#include <numbers>

#include "procedural_ir.h"
#include "procedural_mesher.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

	ProceduralWalkingCreature::ProceduralWalkingCreature(int id, float x, float y, float z, float length):
		Shape(), length_(length) {
		SetId(id);
		width_ = length * 0.8f;
		height_ = length * 0.74f;
		current_pos_ = glm::vec3(x, y, z);
		target_pos_ = current_pos_;

		ProceduralIR ir = GenerateIR();
		model_ = ProceduralMesher::GenerateModel(ir);
		model_->UpdateAnimation(0.0f);
		model_->SetPosition(x, y, z);

		std::string names[4] = {"FL", "FR", "BR", "BL"};

		// Geometry constants (shared between constraints and rest offsets)
		float upper_horiz = length_ * 0.35f;
		float upper_arch = height_ * 0.4f;
		float knee_y = height_ * 0.8f + upper_arch;
		float foot_h_rest = length_ * 0.08f;
		float drop = knee_y - foot_h_rest;
		float lower_horiz = drop * std::tan(glm::radians(15.0f));
		float hip_lateral = width_ * 0.64f;

		// Compute hinge rest angle from bind-pose geometry.
		// Use positive-X convention (right side); per-side axis flip makes the
		// signed angle identical for both sides.
		glm::vec3 upper_dir = glm::normalize(glm::vec3(upper_horiz, upper_arch, 0));
		glm::vec3 lower_dir = glm::normalize(glm::vec3(lower_horiz, -drop, 0));
		float     rest_dot = glm::clamp(glm::dot(upper_dir, lower_dir), -1.0f, 1.0f);
		float     rest_angle = glm::degrees(std::acos(rest_dot));
		// Sign: cross_z = upper_horiz*(-drop) - upper_arch*lower_horiz < 0
		// With right-side axis (0,0,-1): dot(cross,(0,0,-1)) > 0 → positive angle.
		// With left-side axis (0,0,1): actual left-side dirs give same positive angle.
		// So unsigned acos value is correct for both sides.

		// // Setup leg constraints
		// for (int i = 0; i < 4; ++i) {
		// 	bool is_left = (i == 0 || i == 3); // FL=0, BL=3

		// 	// Upper leg: twist-only — full directional freedom, prevents axial roll
		// 	BoneConstraint upper_c;
		// 	upper_c.minTwist = -15.0f;
		// 	upper_c.maxTwist = 15.0f;
		// 	model_->SetBoneConstraint(names[i] + "_upper", upper_c);

		// 	// Lower leg: hinge in bending plane, rest-angle-relative limits
		// 	BoneConstraint lower_c;
		// 	lower_c.type = ConstraintType::Hinge;
		// 	lower_c.axis = is_left ? glm::vec3(0, 0, 1) : glm::vec3(0, 0, -1);
		// 	lower_c.restAngle = rest_angle;
		// 	lower_c.minAngle = -60.0f;  // Allow 60° more bending from rest
		// 	lower_c.maxAngle = 60.0f;   // Allow 60° straightening from rest
		// 	lower_c.minTwist = -5.0f;
		// 	lower_c.maxTwist = 5.0f;
		// 	model_->SetBoneConstraint(names[i] + "_lower", lower_c);
		// }

		// Setup legs tracking — rest offsets match GenerateIR foot positions exactly

		// Setup constraints
		std::string bone_names[4] = {"FL", "FR", "BR", "BL"};
		for (int i = 0; i < 4; ++i) {
			BoneConstraint upper_c;
			upper_c.type = ConstraintType::Cone;
			upper_c.coneAngle = 45.0f;
			model_->SetBoneConstraint(bone_names[i] + "_upper", upper_c);

			BoneConstraint lower_c;
			lower_c.type = ConstraintType::Hinge;
			lower_c.axis = (i == 0 || i == 3) ? glm::vec3(0, 0, 1) : glm::vec3(0, 0, -1);
			lower_c.minAngle = -90.0f;
			lower_c.maxAngle = 90.0f;
			model_->SetBoneConstraint(bone_names[i] + "_lower", lower_c);
		}

		legs_.resize(4);
		float total_lateral = hip_lateral + upper_horiz + lower_horiz;
		float z_offset = length_ * 0.64f;

		glm::vec3 offsets[4] = {
			{-total_lateral, 0, z_offset},
			{total_lateral, 0, z_offset},
			{total_lateral, 0, -z_offset},
			{-total_lateral, 0, -z_offset}
		};

		for (int i = 0; i < 4; ++i) {
			legs_[i].name = names[i];
			legs_[i].effector_name = names[i] + "_foot";
			legs_[i].rest_offset = offsets[i];
			legs_[i].world_foot_pos = current_pos_ + offsets[i];
			legs_[i].world_foot_pos.y = 0;
			legs_[i].step_start_pos = legs_[i].world_foot_pos;
			legs_[i].step_target_pos = legs_[i].world_foot_pos;
		}
	}

	ProceduralIR ProceduralWalkingCreature::GenerateIR() {
		ProceduralIR ir;
		ir.name = "critter";

		glm::vec3 body_col(0.6f, 0.6f, 0.7f);
		glm::vec3 leg_col(0.4f, 0.4f, 0.45f);
		glm::vec3 foot_col(0.2f, 0.2f, 0.2f);

		// Body: A Box - The Root Bone
		int body = ir.AddBox(
			glm::vec3(0, height_, 0),
			glm::quat(1, 0, 0, 0),
			glm::vec3(width_ * 0.5f, height_ * 0.3f, length_ * 0.5f),
			body_col,
			-1,
			"body",
			true,
			SkinningMode::Rigid
		);

		std::string names[4] = {"FL", "FR", "BR", "BL"};
		glm::vec3   offsets[4] = {
			{-width_ * 0.64f, height_ * 0.8f, length_ * 0.64f},
			{width_ * 0.64f, height_ * 0.8f, length_ * 0.64f},
			{width_ * 0.64f, height_ * 0.8f, -length_ * 0.64f},
			{-width_ * 0.64f, height_ * 0.8f, -length_ * 0.64f}
		};

		for (int i = 0; i < 4; ++i) {
			// Hip hub - Increased size for better joint coverage
			int hip = ir.AddHub(offsets[i], length_ * 0.12f, leg_col, body, names[i] + "_hip", true, SkinningMode::Smooth);

			float     out_sign = (offsets[i].x > 0) ? 1.0f : -1.0f;
			float     upper_horiz = length_ * 0.4f;
			float     upper_arch = height_ * 0.45f;
			glm::vec3 upper_end = offsets[i] + glm::vec3(out_sign * upper_horiz, upper_arch, 0);
			int       upper = ir.AddTube(
				offsets[i],
				upper_end,
				length_ * 0.11f,
				length_ * 0.09f,
				leg_col,
				hip,
				names[i] + "_upper",
				true,
				SkinningMode::Smooth
			);

			float     foot_h = length_ * 0.08f;
			float     knee_y = upper_end.y;
			float     drop = knee_y - foot_h;
			float     lower_h = drop * std::tan(glm::radians(15.0f));
			glm::vec3 lower_end = upper_end + glm::vec3(out_sign * lower_h, -drop, 0);
			int       lower = ir.AddTube(
				upper_end,
				lower_end,
				length_ * 0.09f,
				length_ * 0.07f,
				leg_col,
				upper,
				names[i] + "_lower",
				true,
				SkinningMode::Smooth
			);

			ir.AddWedge(
				lower_end + glm::vec3(0, -foot_h, 0),
				glm::quat(1, 0, 0, 0),
				glm::vec3(length_ * 0.14f, foot_h, length_ * 0.2f),
				foot_col,
				lower,
				names[i] + "_foot",
				true
			);
		}

		return ir;
	}

	void ProceduralWalkingCreature::Update(float delta_time) {
		current_pos_ = glm::vec3(GetX(), GetY(), GetZ());

		// Get terrain generator from service locator
		auto terrain_gen = ServiceLocator::Instance().Get<ITerrainGenerator>();
		float terrain_h = 0.0f;
		if (terrain_gen) {
			auto [h, norm] = terrain_gen->GetTerrainPropertiesAtPoint(current_pos_.x, current_pos_.z);
			terrain_h = h;
		}

		UpdateMovement(delta_time);

		// Height oscillation and terrain clamping
		float oscillation = 0;
		if(is_walking_) oscillation = std::sin(walk_phi_ * std::numbers::pi_v<float> * 2.0f) * length_ * 0.05f;
		current_pos_.y = terrain_h + oscillation + height_ * 0.2f;

		SetPosition(current_pos_.x, current_pos_.y, current_pos_.z);

		model_->SetPosition(current_pos_.x, current_pos_.y, current_pos_.z);
		model_->SetRotation(glm::angleAxis(glm::radians(current_yaw_), glm::vec3(0, 1, 0)));

		// Apply multi-effector IK
		std::vector<std::string> effectors;
		std::vector<glm::vec3> targets;
		for (auto& leg : legs_) {
			effectors.push_back(leg.effector_name);
			// Ground the feet on the terrain
			if (terrain_gen) {
				auto [fh, fnorm] = terrain_gen->GetTerrainPropertiesAtPoint(leg.world_foot_pos.x, leg.world_foot_pos.z);
				leg.world_foot_pos.y = fh;
			}
			targets.push_back(leg.world_foot_pos);
		}

		model_->SolveIK(effectors, targets, 0.01f, 20, "body", {}, true);
		model_->UpdateAnimation(delta_time);
	}

	void ProceduralWalkingCreature::UpdateMovement(float delta_time) {
		glm::vec3 diff = target_pos_ - current_pos_;
		diff.y = 0;
		float dist = glm::length(diff);

		if (dist > 0.1f) {
			is_walking_ = true;
			glm::vec3 dir = glm::normalize(diff);
			float     target_yaw = glm::degrees(std::atan2(dir.x, dir.z));

			float yaw_diff = target_yaw - current_yaw_;
			while (yaw_diff > 180.0f) yaw_diff -= 360.0f;
			while (yaw_diff < -180.0f) yaw_diff += 360.0f;
			current_yaw_ += yaw_diff * std::min(1.0f, delta_time * 3.0f);

			float speed = length_ * 1.2f;
			current_pos_ += glm::vec3(std::sin(glm::radians(current_yaw_)), 0, std::cos(glm::radians(current_yaw_))) *
				speed * delta_time;
		} else {
			is_walking_ = false;
		}

		if (is_walking_) {
			walk_phi_ += (delta_time / step_duration_);
			if (walk_phi_ >= 4.0f) walk_phi_ -= 4.0f;
		} else {
			walk_phi_ = 0;
		}

		glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(current_yaw_), glm::vec3(0, 1, 0));
		glm::vec3 forward = glm::vec3(rotation * glm::vec4(0, 0, 1, 0));

		// Crawl sequence: FL(0), BR(2), FR(1), BL(3)
		int crawl_sequence[4] = {0, 2, 1, 3};

		for (int j = 0; j < 4; ++j) {
			int   leg_idx = crawl_sequence[j];
			float leg_phi_start = (float)j;
			float leg_phi = walk_phi_ - leg_phi_start;
			while (leg_phi < 0) leg_phi += 4.0f;
			while (leg_phi >= 4.0f) leg_phi -= 4.0f;

			if (is_walking_ && leg_phi < 1.0f) {
				if (!legs_[leg_idx].is_moving) {
					legs_[leg_idx].is_moving = true;
					legs_[leg_idx].step_start_pos = legs_[leg_idx].world_foot_pos;

					glm::vec3 rotated_offset = glm::vec3(rotation * glm::vec4(legs_[leg_idx].rest_offset, 1.0f));
					float     step_dist = length_ * 0.8f;
					legs_[leg_idx].step_target_pos = current_pos_ + rotated_offset + forward * step_dist;
					legs_[leg_idx].step_target_pos.y = current_pos_.y;
				}

				float p = leg_phi;
				float h = std::sin(p * std::numbers::pi_v<float>) * length_ * 0.25f;
				legs_[leg_idx].world_foot_pos = glm::mix(legs_[leg_idx].step_start_pos, legs_[leg_idx].step_target_pos, p);
				legs_[leg_idx].world_foot_pos.y = current_pos_.y + h;
			} else {
				if (legs_[leg_idx].is_moving) {
					legs_[leg_idx].is_moving = false;
					legs_[leg_idx].world_foot_pos = legs_[leg_idx].step_target_pos;
					legs_[leg_idx].world_foot_pos.y = current_pos_.y;
				}
				// While not moving, try to keep foot at its target but glued to ground
				legs_[leg_idx].world_foot_pos.y = current_pos_.y;
			}
		}

		// Adjust body height based on average foot height to stay level
		float avg_y = 0;
		for(auto& leg : legs_) avg_y += leg.world_foot_pos.y;
		avg_y /= 4.0f;
		// If walking, slightly oscillate body height
		if(is_walking_) current_pos_.y = avg_y + std::sin(walk_phi_ * std::numbers::pi_v<float> * 2.0f) * length_ * 0.05f;
	}

	void ProceduralWalkingCreature::PrepareResources(Megabuffer* megabuffer) const {
		if (model_)
			model_->PrepareResources(megabuffer);
	}

	void ProceduralWalkingCreature::render() const {
		if (model_)
			model_->render();
	}

	void ProceduralWalkingCreature::render(Shader& shader, const glm::mat4& model_matrix) const {
		if (model_)
			model_->render(shader, model_matrix);
	}

	void ProceduralWalkingCreature::GenerateRenderPackets(
		std::vector<RenderPacket>& out_packets,
		const RenderContext&       context
	) const {
		if (model_)
			model_->GenerateRenderPackets(out_packets, context);
	}

	glm::mat4 ProceduralWalkingCreature::GetModelMatrix() const {
		if (model_)
			return model_->GetModelMatrix();
		return glm::mat4(1.0f);
	}

	AABB ProceduralWalkingCreature::GetAABB() const {
		if (model_)
			return model_->GetAABB();
		return AABB();
	}

} // namespace Boidsish
