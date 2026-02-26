#include "walking_creature.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <numbers>
#include <cmath>
#include <algorithm>

namespace Boidsish {

WalkingCreature::WalkingCreature(int id, float x, float y, float z, float length)
    : Graph(id, x, y, z), length_(length) {

    width_ = length * 0.6f;
    height_ = length * 0.4f;
    current_pos_ = glm::vec3(x, y, z);
    target_pos_ = current_pos_;
    camera_pos_ = current_pos_ + glm::vec3(0, 5, 10);
    look_target_pos_ = current_pos_ + glm::vec3(0, 0, 10);

    // Add Body Node - significantly larger than others
    body_node_idx_ = AddVertex(Vector3(0, height_, 0), length * 0.5f, 0.8f, 0.4f, 0.2f, 1.0f).GetId();

    // Add Head Node
    head_node_idx_ = AddVertex(Vector3(0, height_ + length * 0.4f, length * 0.2f), length * 0.25f, 0.9f, 0.5f, 0.3f, 1.0f).GetId();
    AddEdge(V(body_node_idx_), V(head_node_idx_));

    // Define Leg Rest Offsets (relative to body center)
    // 0: FL, 1: FR, 2: BR, 3: BL
    legs_[0].rest_offset = glm::vec3(-width_ / 2.0f, -height_, length_ / 2.0f);
    legs_[1].rest_offset = glm::vec3(width_ / 2.0f, -height_, length_ / 2.0f);
    legs_[2].rest_offset = glm::vec3(width_ / 2.0f, -height_, -length_ / 2.0f);
    legs_[3].rest_offset = glm::vec3(-width_ / 2.0f, -height_, -length_ / 2.0f);

    for (int i = 0; i < 4; ++i) {
        legs_[i].world_foot_pos = current_pos_ + legs_[i].rest_offset;
        legs_[i].node_idx = AddVertex(Vector3(legs_[i].rest_offset), length * 0.2f, 0.6f, 0.3f, 0.1f, 1.0f).GetId();
        AddEdge(V(body_node_idx_), V(legs_[i].node_idx));
    }
}

void WalkingCreature::Update(float delta_time) {
    UpdateMovement(delta_time);
    UpdateNodes();
    MarkDirty();
}

void WalkingCreature::UpdateMovement(float delta_time) {
    glm::vec3 diff = target_pos_ - current_pos_;
    diff.y = 0;
    float dist = glm::length(diff);

    if (dist > 0.1f) {
        is_walking_ = true;
        glm::vec3 dir = glm::normalize(diff);
        float target_yaw = glm::degrees(std::atan2(dir.x, dir.z));

        float yaw_diff = target_yaw - current_yaw_;
        while (yaw_diff > 180.0f) yaw_diff -= 360.0f;
        while (yaw_diff < -180.0f) yaw_diff += 360.0f;
        current_yaw_ += yaw_diff * std::min(1.0f, delta_time * 3.0f);

        float step_length = 0.25f * length_;
        float speed = step_length / (2.0f * step_duration_);
        current_pos_ += glm::vec3(std::sin(glm::radians(current_yaw_)), 0, std::cos(glm::radians(current_yaw_))) * speed * delta_time;
    } else {
        is_walking_ = false;
        // Optionally reset walk_phi_ slowly or just keep it
    }

    if (is_walking_) {
        walk_phi_ += (delta_time / step_duration_) * 0.5f;
        if (walk_phi_ >= 2.0f) walk_phi_ -= 2.0f;

        for (int j = 0; j < 4; ++j) {
            int leg_idx = sequence_[j];
            float leg_phi_start = j * 0.5f;
            float leg_phi = walk_phi_ - leg_phi_start;
            if (leg_phi < 0) leg_phi += 2.0f;

            if (leg_phi < 1.0f) { // Leg is moving
                if (!legs_[leg_idx].is_moving) {
                    legs_[leg_idx].is_moving = true;
                    legs_[leg_idx].step_start_pos = legs_[leg_idx].world_foot_pos;

                    float step_length = 0.25f * length_;
                    glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(current_yaw_), glm::vec3(0, 1, 0));
                    glm::vec3 rotated_offset = glm::vec3(rotation * glm::vec4(legs_[leg_idx].rest_offset, 1.0f));
                    // Target is rest offset + half step length forward
                    legs_[leg_idx].step_target_pos = current_pos_ + rotated_offset + glm::vec3(std::sin(glm::radians(current_yaw_)), 0, std::cos(glm::radians(current_yaw_))) * (step_length * 0.5f);
                }

                legs_[leg_idx].progress = leg_phi;
                float p = legs_[leg_idx].progress;
                float theta = 2.0f * std::numbers::pi_v<float> * p;

                // Cycloid formulas
                float horizontal_p = (theta - std::sin(theta)) / (2.0f * std::numbers::pi_v<float>);
                float vertical_p = (1.0f - std::cos(theta)) / 2.0f;

                float step_height = length_ * 0.15f;
                legs_[leg_idx].world_foot_pos = glm::mix(legs_[leg_idx].step_start_pos, legs_[leg_idx].step_target_pos, horizontal_p);
                legs_[leg_idx].world_foot_pos.y = current_pos_.y + vertical_p * step_height;
            } else {
                legs_[leg_idx].is_moving = false;
                legs_[leg_idx].progress = 1.0f;
            }
        }
    }

    // Head following logic
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
}

void WalkingCreature::UpdateNodes() {
    SetPosition(current_pos_.x, current_pos_.y, current_pos_.z);

    glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(current_yaw_), glm::vec3(0, 1, 0));

    // Update Body
    V(body_node_idx_).position = Vector3(0, height_, 0);

    // Update Legs
    for (int i = 0; i < 4; ++i) {
        glm::vec3 rel_foot = legs_[i].world_foot_pos - current_pos_;
        V(legs_[i].node_idx).position = Vector3(rel_foot);
    }

    // Head
    glm::vec3 head_base_local(0, height_, length_ * 0.1f);
    glm::vec3 head_base_world = current_pos_ + glm::vec3(rotation * glm::vec4(head_base_local, 1.0f));

    glm::vec3 target = is_looking_at_camera_ ? camera_pos_ : look_target_pos_;
    glm::vec3 look_dir = glm::normalize(target - head_base_world);

    // The head "neck" edge
    glm::vec3 head_world = head_base_world + look_dir * (length_ * 0.4f);
    // Ensure head is above ground and generally above body
    if (head_world.y < current_pos_.y + height_ + length_ * 0.1f) {
        head_world.y = current_pos_.y + height_ + length_ * 0.1f;
    }

    V(head_node_idx_).position = Vector3(head_world - current_pos_);
}

}
