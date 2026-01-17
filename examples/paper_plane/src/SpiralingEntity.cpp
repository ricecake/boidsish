#include "SpiralingEntity.h"
#include "PaperPlane.h"
#include "graphics.h"
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

SpiralingEntity::SpiralingEntity(int id, Vector3 pos)
    : Entity<Model>(id, "assets/Missile.obj", true),
      rotational_velocity_(glm::vec3(0.0f)),
      forward_speed_(100.0f),
      eng_(rd_()) {
    SetPosition(pos.x, pos.y, pos.z);
    SetVelocity(0, 0, 0);
    SetTrailLength(500);
    shape_->SetScale(glm::vec3(0.08f));
    std::dynamic_pointer_cast<Model>(shape_)->SetBaseRotation(
        glm::angleAxis(glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)));

    std::uniform_int_distribution<> dist(0, 1);
    handedness_ = dist(eng_);
}

void SpiralingEntity::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
    auto targets = handler.GetEntitiesByType<PaperPlane>();
    if (targets.empty()) {
        // No target, fly straight
        rotational_velocity_ = glm::vec3(0.0f);
    } else {
        auto plane = targets[0];

        Vector3   target_vec = (plane->GetPosition() - GetPosition());
        float distance = target_vec.Magnitude();
        target_vec = target_vec.Normalized();

        glm::vec3 target_dir_world = glm::vec3(target_vec.x, target_vec.y, target_vec.z);
        glm::vec3 forward_dir = orientation_ * glm::vec3(0.0f, 0.0f, 1.0f);
        float dot_product = glm::dot(glm::normalize(forward_dir), glm::normalize(target_dir_world));

        const float kTurnSpeed = 4.0f;
        const float kDamping = 2.5f;

        state_timer_ += delta_time;

        if (current_state_ == FlightState::Homing) {
            if (distance < 300.0f) {
                current_state_ = FlightState::Spiraling;
                state_timer_ = 0.0f;
            }
        } else if (current_state_ == FlightState::Spiraling) {
            if (distance < 50.0f) {
                current_state_ = FlightState::Breaking;
                state_timer_ = 0.0f;
            }
        } else if (current_state_ == FlightState::Breaking) {
            if (state_timer_ > 1.0f) {
                current_state_ = FlightState::Looping;
                state_timer_ = 0.0f;
            }
        } else if (current_state_ == FlightState::Looping) {
            if (state_timer_ > 3.0f && dot_product > 0.8f) {
                current_state_ = FlightState::Homing;
                state_timer_ = 0.0f;
            }
        }

        glm::vec3 target_rot_velocity = glm::vec3(0.0f);

        if (current_state_ == FlightState::Homing) {
            glm::vec3 target_dir_local = glm::inverse(orientation_) * target_dir_world;
            target_rot_velocity.y = target_dir_local.x * kTurnSpeed;
            target_rot_velocity.x = -target_dir_local.y * kTurnSpeed;
        } else if (current_state_ == FlightState::Spiraling) {
            float spiral_factor = 1.0f - (distance / 300.0f);
            glm::vec3 target_dir_local = glm::inverse(orientation_) * target_dir_world;
            target_rot_velocity.y = target_dir_local.x * kTurnSpeed;
            target_rot_velocity.x = -target_dir_local.y * kTurnSpeed;
            target_rot_velocity.y += (handedness_ ? 1.0f : -1.0f) * spiral_factor * 5.0f;
        } else if (current_state_ == FlightState::Breaking) {
            target_rot_velocity.y = (handedness_ ? 1.0f : -1.0f) * kTurnSpeed * 2.0f;
            target_rot_velocity.x = -kTurnSpeed;
        } else if (current_state_ == FlightState::Looping) {
            target_rot_velocity.y = (handedness_ ? 1.0f : -1.0f) * kTurnSpeed;
            target_rot_velocity.x = kTurnSpeed;
        }

        rotational_velocity_ += (target_rot_velocity - rotational_velocity_) * kDamping * delta_time;
    }

    glm::quat pitch_delta = glm::angleAxis(rotational_velocity_.x * delta_time, glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat yaw_delta = glm::angleAxis(rotational_velocity_.y * delta_time, glm::vec3(0.0f, 1.0f, 0.0f));
    orientation_ = glm::normalize(orientation_ * pitch_delta * yaw_delta);

    glm::vec3 forward_dir = orientation_ * glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 new_velocity = forward_dir * forward_speed_;
    SetVelocity(Vector3(new_velocity.x, new_velocity.y, new_velocity.z));
}

void SpiralingEntity::UpdateShape() {
    Entity<Model>::UpdateShape();
    if (shape_) {
        shape_->SetRotation(orientation_);
    }
}

} // namespace Boidsish
