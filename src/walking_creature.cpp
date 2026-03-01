#include "walking_creature.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <numbers>

#include "animator.h"
#include "procedural_generator.h"
#include "procedural_mesher.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

	namespace {
		std::shared_ptr<ModelData> CreateCreatureData(float length, float width, float height) {
			ProceduralIR ir;
			ir.name = "critter";

			glm::vec3 bodyColor(0.4f, 0.45f, 0.5f);
			glm::vec3 legColor(0.3f, 0.32f, 0.35f);

			int bodyIdx = ir.AddHub(glm::vec3(0, height, 0), length * 0.5f, bodyColor, -1, "body");

			const char* legPrefixes[] = {"FL", "FR", "BR", "BL"};
			glm::vec3   offsets[] = {
                {-width * 0.7f, 0.0f, length * 0.4f},
                {width * 0.7f, 0.0f, length * 0.4f},
                {width * 0.7f, 0.0f, -length * 0.4f},
                {-width * 0.7f, 0.0f, -length * 0.4f}
            };

			for (int i = 0; i < 4; ++i) {
				std::string prefix = legPrefixes[i];
				glm::vec3   basePos = glm::vec3(0, height, 0) + offsets[i] * 0.6f;
				glm::vec3   kneePos = basePos + glm::vec3(offsets[i].x * 0.8f, height * 0.2f, offsets[i].z * 0.2f);
				glm::vec3   footPos = basePos + glm::vec3(offsets[i].x * 1.2f, -height, offsets[i].z * 0.5f);

				int upperIdx = ir.AddTube(basePos, kneePos, length * 0.15f, length * 0.12f, legColor, bodyIdx, prefix + "_upper");
				ir.AddHub(basePos, length * 0.15f, legColor, upperIdx); // Visual hip joint attached to upper leg
				int lowerIdx = ir.AddTube(kneePos, footPos, length * 0.12f, length * 0.1f, legColor, upperIdx, prefix + "_lower");
				ir.AddControlPoint(footPos, length * 0.1f, legColor, lowerIdx, prefix + "_effector");
				ir.AddPuffball(footPos, length * 0.15f, legColor, 0, lowerIdx);
			}

			auto model = ProceduralMesher::GenerateModel(ir);
			return model->GetData();
		}
	}

	WalkingCreature::WalkingCreature(int id, float x, float y, float z, float length):
		Model(CreateCreatureData(length, length * 1.2f, length * 0.3f)), length_(length) {
		SetId(id);
		SetPosition(x, y, z);
		width_ = length * 1.2f;
		height_ = length * 0.3f;
		current_pos_ = glm::vec3(x, y, z);
		target_pos_ = current_pos_;
		camera_pos_ = current_pos_ + glm::vec3(0, 5, 10);

		spotlight_ = Light::CreateSpot(current_pos_, glm::vec3(0, 0, 1), 40.0f, glm::vec3(0.5f, 0.7f, 1.0f), 25.0f, 35.0f);

		const char* legPrefixes[] = {"FL", "FR", "BR", "BL"};
		glm::vec3   offsets[] = {
            {-width_ * 0.7f, 0.0f, length_ * 0.4f},
            {width_ * 0.7f, 0.0f, length_ * 0.4f},
            {width_ * 0.7f, 0.0f, -length_ * 0.4f},
            {-width_ * 0.7f, 0.0f, -length_ * 0.4f}
        };

		for (int i = 0; i < 4; ++i) {
			legs_[i].bone_name = "body";
			legs_[i].thigh_bone_name = std::string(legPrefixes[i]) + "_upper";
			legs_[i].knee_bone_name = std::string(legPrefixes[i]) + "_lower";
			legs_[i].foot_bone_name = std::string(legPrefixes[i]) + "_effector";
			legs_[i].rest_offset = offsets[i] + glm::vec3(0, -height_, 0);
			legs_[i].world_foot_pos = current_pos_ + legs_[i].rest_offset;
			legs_[i].world_foot_pos.y = current_pos_.y;

			BoneConstraint upperConstraint;
			upperConstraint.type = ConstraintType::Cone;
			upperConstraint.coneAngle = 30.0f;
            upperConstraint.maxTwistAngle = 10.0f;
			SetBoneConstraint(legs_[i].thigh_bone_name, upperConstraint);

            BoneConstraint lowerConstraint;
            lowerConstraint.type = ConstraintType::Hinge;
            lowerConstraint.axis = glm::normalize(glm::cross(glm::vec3(0, 1, 0), offsets[i]));
            lowerConstraint.minAngle = 0.0f;
            lowerConstraint.maxAngle = 90.0f;
            lowerConstraint.maxTwistAngle = 5.0f;
            SetBoneConstraint(legs_[i].knee_bone_name, lowerConstraint);
		}
	}

	void WalkingCreature::Update(float delta_time) {
		current_pos_ = glm::vec3(GetX(), GetY(), GetZ());
		UpdateMovement(delta_time);
		UpdateBalance(delta_time);
		UpdateSkeleton(delta_time);
		MarkDirty();
	}

	void WalkingCreature::UpdateMovement(float delta_time) {
		glm::vec3 diff = target_pos_ - current_pos_;
		diff.y = 0;
		float dist = glm::length(diff);

		glm::vec3 velocity(0.0f);
		if (dist > 0.1f) {
			is_walking_ = true;
			glm::vec3 dir = glm::normalize(diff);
			float     target_yaw = glm::degrees(std::atan2(dir.x, dir.z));

			float yaw_diff = target_yaw - current_yaw_;
			while (yaw_diff > 180.0f)
				yaw_diff -= 360.0f;
			while (yaw_diff < -180.0f)
				yaw_diff += 360.0f;
			current_yaw_ += yaw_diff * (1.0f - std::exp(-delta_time * 3.0f));

			float step_length = 0.25f * length_;
			float speed = step_length / (4.0f * step_duration_);
			velocity = glm::vec3(std::sin(glm::radians(current_yaw_)), 0, std::cos(glm::radians(current_yaw_))) * speed;
			current_pos_ += velocity * delta_time;
			SetPosition(current_pos_.x, current_pos_.y, current_pos_.z);
			SetRotation(glm::angleAxis(glm::radians(current_yaw_), glm::vec3(0, 1, 0)));
		} else {
			is_walking_ = false;
		}

		glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(current_yaw_), glm::vec3(0, 1, 0));

		int moving_count = 0;
		for (int i = 0; i < 4; ++i)
			if (legs_[i].is_moving)
				moving_count++;

		// Creep gait: only one leg moves at a time
		if (moving_count == 0) {
			float max_dist = 0;
			int   best_leg = -1;

			for (int i = 0; i < 4; ++i) {
				glm::vec3 ideal_world_foot = current_pos_ + glm::vec3(rotation * glm::vec4(legs_[i].rest_offset.x, 0.0f, legs_[i].rest_offset.z, 1.0f));
				float     d = glm::distance(legs_[i].world_foot_pos, ideal_world_foot);
				if (d > max_dist) {
					max_dist = d;
					best_leg = i;
				}
			}

			if (best_leg != -1 && (max_dist > length_ * 0.25f || !is_walking_)) {
				legs_[best_leg].is_moving = true;
				legs_[best_leg].progress = 0.0f;
				legs_[best_leg].step_start_pos = legs_[best_leg].world_foot_pos;
				glm::vec3 local_ideal = current_pos_ + glm::vec3(rotation * glm::vec4(legs_[best_leg].rest_offset.x, 0.0f, legs_[best_leg].rest_offset.z, 1.0f));
				legs_[best_leg].step_target_pos = local_ideal + velocity * step_duration_ * 3.0f;
				legs_[best_leg].step_target_pos.y = current_pos_.y;
			}
		}

		for (int i = 0; i < 4; ++i) {
			if (legs_[i].is_moving) {
				legs_[i].progress += delta_time / step_duration_;
				if (legs_[i].progress >= 1.0f) {
					legs_[i].progress = 1.0f;
					legs_[i].is_moving = false;
					legs_[i].world_foot_pos = legs_[i].step_target_pos;
					legs_[i].world_foot_pos.y = current_pos_.y;
				} else {
					float p = legs_[i].progress;
					float height_offset = std::sin(p * std::numbers::pi_v<float>) * length_ * 0.15f;
					legs_[i].world_foot_pos = glm::mix(legs_[i].step_start_pos, legs_[i].step_target_pos, p);
					legs_[i].world_foot_pos.y = current_pos_.y + height_offset;
				}
			} else {
				legs_[i].world_foot_pos.y = current_pos_.y;
			}
		}
	}

	void WalkingCreature::UpdateBalance(float delta_time) {
		glm::vec3 com = current_pos_ + glm::vec3(0, height_, 0);
		glm::vec3 support_center(0.0f);
		int       planted_count = 0;
		for (int i = 0; i < 4; ++i) {
			if (!legs_[i].is_moving) {
				support_center += legs_[i].world_foot_pos;
				planted_count++;
			}
		}

		if (planted_count > 0) {
			support_center /= (float)planted_count;
			glm::vec3 target_body_pos = support_center + glm::vec3(0, height_, 0);
			glm::vec3 diff = target_body_pos - com;
			diff.y = 0;
			body_offset_ = glm::mix(body_offset_, diff, (1.0f - std::exp(-delta_time * 5.0f)));

            // Safety: clamp body offset
            float max_off = length_ * 0.5f;
            if (glm::length(body_offset_) > max_off) body_offset_ = glm::normalize(body_offset_) * max_off;
		}
	}

	void WalkingCreature::UpdateSkeleton(float delta_time) {
		glm::mat4 bodyTransform = GetAnimator()->GetBoneLocalTransform("body");
		bodyTransform[3] = glm::vec4(glm::vec3(0, height_, 0) + body_offset_, 1.0f);
		GetAnimator()->SetBoneLocalTransform("body", bodyTransform);

		// Ensure global matrices are updated so SolveIK starts from the correct hip positions
		UpdateAnimation(delta_time);

		for (int i = 0; i < 4; ++i) {
			SolveIK(legs_[i].foot_bone_name, legs_[i].world_foot_pos, 0.01f, 30, legs_[i].bone_name, {"body", legs_[i].thigh_bone_name});
		}

		spotlight_.position = GetBoneWorldPosition("body");
		glm::mat4 bodyRot = glm::mat4_cast(GetRotation());
		spotlight_.direction = glm::normalize(glm::vec3(bodyRot * glm::vec4(0, -0.5f, 1, 0)));

		UpdateAnimation(delta_time);
	}

} // namespace Boidsish
