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

			glm::vec3 bodyColor(0.8f, 0.4f, 0.2f);
			glm::vec3 legColor(0.7f, 0.35f, 0.15f);

			// Body
			int bodyIdx = ir.AddHub(glm::vec3(0, height, 0), length * 0.4f, bodyColor, -1, "body");

			// Legs
			const char* legNames[] = {"FL", "FR", "BR", "BL"};
			glm::vec3   offsets[] = {
                {-width / 2.0f, 0.0f, length / 2.0f},
                {width / 2.0f, 0.0f, length / 2.0f},
                {width / 2.0f, 0.0f, -length / 2.0f},
                {-width / 2.0f, 0.0f, -length / 2.0f}
            };

			for (int i = 0; i < 4; ++i) {
				std::string name = legNames[i];
				glm::vec3   basePos = glm::vec3(0, height, 0) + offsets[i];

				// Hip
				int hipIdx = ir.AddHub(basePos, length * 0.1f, legColor, bodyIdx, name + "_hip");

				// Upper leg
				glm::vec3 kneePos = basePos + glm::vec3(offsets[i].x * 0.5f, -height * 0.5f, 0);
				int kneeIdx = ir.AddTube(basePos, kneePos, length * 0.08f, length * 0.06f, legColor, hipIdx, name + "_upper");

				// Lower leg
				glm::vec3 footPos = basePos + glm::vec3(0, -height, 0);
				int footIdx = ir.AddTube(kneePos, footPos, length * 0.06f, length * 0.04f, legColor, kneeIdx, name + "_lower");

				// Foot hub
				ir.AddHub(footPos, length * 0.08f, legColor, footIdx, name + "_foot");
			}

			// Neck and Head
			glm::vec3 neckBase(0, height, length * 0.3f);
			int neckIdx = ir.AddTube(neckBase, neckBase + glm::vec3(0, length * 0.3f, length * 0.2f), length * 0.1f, length * 0.08f, bodyColor, bodyIdx, "neck");
			ir.AddHub(neckBase + glm::vec3(0, length * 0.3f, length * 0.2f), length * 0.2f, bodyColor, neckIdx, "head");

			auto model = ProceduralMesher::GenerateModel(ir);
			return model->GetData();
		}
	}

	WalkingCreature::WalkingCreature(int id, float x, float y, float z, float length):
		Model(CreateCreatureData(length, length * 0.7f, length * 0.5f)), length_(length) {
		SetId(id);
		SetPosition(x, y, z);
		width_ = length * 0.7f;
		height_ = length * 0.5f;
		current_pos_ = glm::vec3(x, y, z);
		target_pos_ = current_pos_;
		camera_pos_ = current_pos_ + glm::vec3(0, 5, 10);
		look_target_pos_ = current_pos_ + glm::vec3(0, 0, 10);
		current_head_dir_ = glm::vec3(0, 0, 1);

		spotlight_ = Light::CreateSpot(current_pos_, glm::vec3(0, 0, 1), 20.0f, glm::vec3(1.0f), 15.0f, 25.0f);

		const char* legPrefixes[] = {"FL", "FR", "BR", "BL"};
		glm::vec3   offsets[] = {
            {-width_ / 2.0f, 0.0f, length_ / 2.0f},
            {width_ / 2.0f, 0.0f, length_ / 2.0f},
            {width_ / 2.0f, 0.0f, -length_ / 2.0f},
            {-width_ / 2.0f, 0.0f, -length_ / 2.0f}
        };

		for (int i = 0; i < 4; ++i) {
			legs_[i].bone_name = std::string(legPrefixes[i]) + "_hip";
			legs_[i].knee_bone_name = std::string(legPrefixes[i]) + "_upper";
			legs_[i].foot_bone_name = std::string(legPrefixes[i]) + "_foot";
			legs_[i].rest_offset = offsets[i] + glm::vec3(0, -height_, 0);
			legs_[i].world_foot_pos = current_pos_ + legs_[i].rest_offset;
			legs_[i].world_foot_pos.y = current_pos_.y; // Clamp to ground initially

			// Apply constraints
			BoneConstraint kneeConstraint;
			kneeConstraint.type = ConstraintType::Hinge;
			kneeConstraint.axis = glm::normalize(glm::cross(glm::vec3(0, 1, 0), offsets[i]));
			kneeConstraint.minAngle = 0.0f;
			kneeConstraint.maxAngle = 120.0f;
			SetBoneConstraint(legs_[i].knee_bone_name, kneeConstraint);
		}

		BoneConstraint neckConstraint;
		neckConstraint.type = ConstraintType::Cone;
		neckConstraint.coneAngle = 45.0f;
		SetBoneConstraint("neck", neckConstraint);
	}

	void WalkingCreature::Update(float delta_time) {
		current_pos_ = glm::vec3(GetX(), GetY(), GetZ());
		UpdateMovement(delta_time);
		UpdateBalance();
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
			current_yaw_ += yaw_diff * std::min(1.0f, delta_time * 3.0f);

			float step_length = 0.25f * length_;
			float speed = step_length / (2.0f * step_duration_);
			velocity = glm::vec3(std::sin(glm::radians(current_yaw_)), 0, std::cos(glm::radians(current_yaw_))) * speed;
			current_pos_ += velocity * delta_time;
			SetPosition(current_pos_.x, current_pos_.y, current_pos_.z);
			SetRotation(glm::angleAxis(glm::radians(current_yaw_), glm::vec3(0, 1, 0)));
		} else {
			is_walking_ = false;
		}

		glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(current_yaw_), glm::vec3(0, 1, 0));

		// Step triggering and animation
		int moving_count = 0;
		for (int i = 0; i < 4; ++i)
			if (legs_[i].is_moving)
				moving_count++;

		for (int i = 0; i < 4; ++i) {
			glm::vec3 ideal_world_foot = current_pos_ + glm::vec3(rotation * glm::vec4(legs_[i].rest_offset.x, 0.0f, legs_[i].rest_offset.z, 1.0f));
			ideal_world_foot.y = current_pos_.y;

			float d = glm::distance(legs_[i].world_foot_pos, ideal_world_foot);

			if (!legs_[i].is_moving && d > length_ * 0.3f && moving_count < 2) {
				// Prevent same-side legs from moving together if possible, or just use a simple alternating gait
				// For 4 legs, we can allow 2 to move if they are diagonal
				bool can_move = true;
				for (int j = 0; j < 4; ++j) {
					if (legs_[j].is_moving) {
						// Diagonal pairs: (0, 2) and (1, 3)
						if ((i % 2) == (j % 2))
							can_move = false;
					}
				}

				if (can_move) {
					legs_[i].is_moving = true;
					legs_[i].progress = 0.0f;
					legs_[i].step_start_pos = legs_[i].world_foot_pos;
					legs_[i].step_target_pos = ideal_world_foot + velocity * step_duration_ * 1.5f;
					legs_[i].step_target_pos.y = current_pos_.y;
					moving_count++;
				}
			}

			if (legs_[i].is_moving) {
				legs_[i].progress += delta_time / step_duration_;
				if (legs_[i].progress >= 1.0f) {
					legs_[i].progress = 1.0f;
					legs_[i].is_moving = false;
					legs_[i].world_foot_pos = legs_[i].step_target_pos;
					legs_[i].world_foot_pos.y = current_pos_.y;
				} else {
					float p = legs_[i].progress;
					// Arching path
					float height_offset = std::sin(p * std::numbers::pi_v<float>) * length_ * 0.2f;
					legs_[i].world_foot_pos = glm::mix(legs_[i].step_start_pos, legs_[i].step_target_pos, p);
					legs_[i].world_foot_pos.y = current_pos_.y + height_offset;
				}
			} else {
				// Ensure planted feet stay on ground if terrain moves (though here it's static usually)
				legs_[i].world_foot_pos.y = current_pos_.y;
			}
		}
	}

	void WalkingCreature::UpdateBalance() {
		// Calculate center of mass (body position)
		glm::vec3 com = current_pos_ + glm::vec3(0, height_, 0);

		// Support polygon is formed by feet on the ground
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
			// We want the body to shift towards the support center but keep its height
			glm::vec3 target_body_pos = support_center + glm::vec3(0, height_, 0);
			glm::vec3 diff = target_body_pos - com;
			diff.y = 0;

			// Apply a small fraction of the difference to the body offset for smooth balancing
			body_offset_ = glm::mix(body_offset_, diff, 0.1f);
		}
	}

	void WalkingCreature::UpdateSkeleton(float delta_time) {
		// Apply body offset
		glm::mat4 bodyTransform = GetAnimator()->GetBoneLocalTransform("body");
		bodyTransform[3] = glm::vec4(glm::vec3(0, height_, 0) + body_offset_, 1.0f);
		GetAnimator()->SetBoneLocalTransform("body", bodyTransform);

		// Update legs using IK
		for (int i = 0; i < 4; ++i) {
			SolveIK(legs_[i].foot_bone_name, legs_[i].world_foot_pos, 0.01f, 10, legs_[i].bone_name);
		}

		// Update Head
		glm::vec3 head_base_world = GetBoneWorldPosition("neck");
		glm::vec3 target = is_looking_at_camera_ ? camera_pos_ : look_target_pos_;
		glm::vec3 target_look_dir = glm::normalize(target - head_base_world);

		current_head_dir_ = glm::normalize(glm::mix(current_head_dir_, target_look_dir, std::min(1.0f, delta_time * 4.0f)));

		glm::vec3 head_target = head_base_world + current_head_dir_ * (length_ * 0.3f);
		if (head_target.y < current_pos_.y + height_ * 0.2f) head_target.y = current_pos_.y + height_ * 0.2f;

		SolveIK("head", head_target, 0.01f, 5, "neck");

		// Update Spotlight
		spotlight_.position = GetBoneWorldPosition("head");
		spotlight_.direction = current_head_dir_;

		// Head looking logic
		look_timer_ -= delta_time;
		if (look_timer_ <= 0) {
			if (is_looking_at_camera_) {
				is_looking_at_camera_ = false;
				look_timer_ = 1.0f + (std::rand() % 200) / 100.0f;
				float angle = (std::rand() % 360) * std::numbers::pi_v<float> / 180.0f;
				look_target_pos_ = current_pos_ + glm::vec3(std::cos(angle), 0, std::sin(angle)) * 10.0f;
			} else {
				is_looking_at_camera_ = true;
				look_timer_ = 2.0f + (std::rand() % 300) / 100.0f;
			}
		}

		// Spotlight random changes
		light_change_timer_ -= delta_time;
		if (light_change_timer_ <= 0.0f) {
			light_change_timer_ = 1.0f + (std::rand() % 300) / 100.0f;
			spotlight_.color = glm::vec3((std::rand() % 100) / 100.0f, (std::rand() % 100) / 100.0f, (std::rand() % 100) / 100.0f);
			if (glm::length(spotlight_.color) < 0.2f)
				spotlight_.color = glm::vec3(1.0f);

			float inner = 5.0f + (std::rand() % 20);
			float outer = inner + 5.0f + (std::rand() % 15);
			spotlight_.inner_cutoff = glm::cos(glm::radians(inner));
			spotlight_.outer_cutoff = glm::cos(glm::radians(outer));
		}

		UpdateAnimation(0.0f);
	}

} // namespace Boidsish
