#include "procedural_walking_creature.h"

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

		// Body: A Box
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
			// Hip hub - NOT a bone (IK will start from upper leg)
			int hip =
				ir.AddHub(offsets[i], length_ * 0.08f, leg_col, body, names[i] + "_hip", false, SkinningMode::Rigid);

			// Upper leg: moderate upward arch, going outward
			// Knee is above the hip so the leg forms a natural arch (knee points up/out)
			float     out_sign = (offsets[i].x > 0) ? 1.0f : -1.0f;
			float     upper_horiz = length_ * 0.35f;
			float     upper_arch = height_ * 0.4f;
			glm::vec3 upper_end = offsets[i] + glm::vec3(out_sign * upper_horiz, upper_arch, 0);
			int       upper = ir.AddTube(
                offsets[i],
                upper_end,
                length_ * 0.1f,
                length_ * 0.08f,
                leg_col,
                hip,
                names[i] + "_upper",
                true,
                SkinningMode::Smooth
            );

			// Lower leg: mostly down, 15° outward tilt to widen stance
			float     foot_h = length_ * 0.08f;
			float     knee_y = upper_end.y;
			float     drop = knee_y - foot_h;
			float     lower_h = drop * std::tan(glm::radians(15.0f));
			glm::vec3 lower_end = upper_end + glm::vec3(out_sign * lower_h, -drop, 0);
			int       lower = ir.AddTube(
                upper_end,
                lower_end,
                length_ * 0.08f,
                length_ * 0.06f,
                leg_col,
                upper,
                names[i] + "_lower",
                true,
                SkinningMode::Smooth
            );

			// Foot: Wedge - bone so it can be the IK end-effector
			ir.AddWedge(
				lower_end + glm::vec3(0, -foot_h, 0),
				glm::quat(1, 0, 0, 0),
				glm::vec3(length_ * 0.12f, foot_h, length_ * 0.18f),
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

		UpdateMovement(delta_time);

		// Update outer shape position so callers/tests see the movement
		SetPosition(current_pos_.x, current_pos_.y, current_pos_.z);

		// Update model transform before IK so world-to-model conversions are correct
		model_->SetPosition(current_pos_.x, current_pos_.y, current_pos_.z);
		model_->SetRotation(glm::angleAxis(glm::radians(current_yaw_), glm::vec3(0, 1, 0)));

		// One-time diagnostic: print bone positions and IK targets
		static bool diag_printed = false;
		if (!diag_printed) {
			diag_printed = true;
			auto print_v = [](const char* label, glm::vec3 v) {
				printf("  %s: (%.3f, %.3f, %.3f)\n", label, v.x, v.y, v.z);
			};
			printf("=== Walking Creature IK Diagnostic ===\n");
			printf("Model pos: (%.3f, %.3f, %.3f)\n", current_pos_.x, current_pos_.y, current_pos_.z);
			for (auto& leg : legs_) {
				printf("Leg %s:\n", leg.name.c_str());
				print_v("  world_foot_target", leg.world_foot_pos);
				print_v("  upper (model-space)", model_->GetBoneWorldPosition(leg.name + "_upper"));
				print_v("  lower (model-space)", model_->GetBoneWorldPosition(leg.name + "_lower"));
				print_v("  foot  (model-space)", model_->GetBoneWorldPosition(leg.name + "_foot"));
			}
			printf("  body (model-space): ");
			auto bp = model_->GetBoneWorldPosition("body");
			printf("(%.3f, %.3f, %.3f)\n", bp.x, bp.y, bp.z);
			printf("======================================\n");
		}

		// Apply IK to position legs on the ground
		// Each leg has its own 3-joint chain: [upper, lower, foot]
		for (auto& leg : legs_) {
			model_->SolveIK(leg.effector_name, leg.world_foot_pos, 0.01f, 20, leg.name + "_upper");
		}
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
			while (yaw_diff > 180.0f)
				yaw_diff -= 360.0f;
			while (yaw_diff < -180.0f)
				yaw_diff += 360.0f;
			current_yaw_ += yaw_diff * std::min(1.0f, delta_time * 3.0f);

			float speed = length_ * 0.75f;
			current_pos_ += glm::vec3(std::sin(glm::radians(current_yaw_)), 0, std::cos(glm::radians(current_yaw_))) *
				speed * delta_time;
		} else {
			is_walking_ = false;
		}

		if (is_walking_) {
			walk_phi_ += (delta_time / step_duration_) * 0.5f;
			if (walk_phi_ >= 2.0f)
				walk_phi_ -= 2.0f;
		}

		glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(current_yaw_), glm::vec3(0, 1, 0));
		glm::vec3 forward = glm::vec3(rotation * glm::vec4(0, 0, 1, 0));

		for (int j = 0; j < 4; ++j) {
			int   leg_idx = sequence_[j];
			float leg_phi_start = j * 0.5f;
			float leg_phi = walk_phi_ - leg_phi_start;
			while (leg_phi < 0)
				leg_phi += 2.0f;
			while (leg_phi >= 2.0f)
				leg_phi -= 2.0f;

			if (is_walking_ && leg_phi < 1.0f) {
				if (!legs_[leg_idx].is_moving) {
					legs_[leg_idx].is_moving = true;
					legs_[leg_idx].step_start_pos = legs_[leg_idx].world_foot_pos;

					glm::vec3 rotated_offset = glm::vec3(rotation * glm::vec4(legs_[leg_idx].rest_offset, 1.0f));
					float     step_dist = length_ * 0.4f;
					legs_[leg_idx].step_target_pos = current_pos_ + rotated_offset + forward * step_dist;
					legs_[leg_idx].step_target_pos.y = current_pos_.y;
				}

				float p = leg_phi;
				float h = std::sin(p * std::numbers::pi_v<float>) * length_ * 0.15f;
				legs_[leg_idx]
					.world_foot_pos = glm::mix(legs_[leg_idx].step_start_pos, legs_[leg_idx].step_target_pos, p);
				legs_[leg_idx].world_foot_pos.y = current_pos_.y + h;
			} else {
				if (legs_[leg_idx].is_moving) {
					legs_[leg_idx].is_moving = false;
					legs_[leg_idx].world_foot_pos = legs_[leg_idx].step_target_pos;
					legs_[leg_idx].world_foot_pos.y = current_pos_.y;
				}
			}
		}
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
