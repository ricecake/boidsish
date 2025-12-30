#include "paper_plane.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Boidsish {

PaperPlane::PaperPlane(int id)
    : Entity<Model>(id, "assets/Mesh_Cat.obj", true),
      orientation_(glm::quat(1.0f, 0.0f, 0.0f, 0.0f)),
      rotational_velocity_(glm::vec3(0.0f)),
      forward_speed_(20.0f) {
    SetTrailLength(150);
    SetTrailIridescence(true);

    SetColor(1.0f, 0.5f, 0.0f);
    shape_->SetScale(glm::vec3(0.04f));
    std::dynamic_pointer_cast<Model>(shape_)->SetBaseRotation(
        glm::angleAxis(glm::radians(-180.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
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

    const float kBaseSpeed = 20.0f;
    const float kBoostSpeed = 80.0f;
    const float kBoostAcceleration = 120.0f;
    const float kSpeedDecay = 10.0f;

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
    // Automatically roll when yawing
    target_rot_velocity.z += target_rot_velocity.y * kCoordinatedTurnFactor;

    // --- Auto-leveling ---
    if (!controller_->pitch_up && !controller_->pitch_down && !controller_->yaw_left &&
        !controller_->yaw_right && !controller_->roll_left && !controller_->roll_right) {
        glm::vec3 world_up_in_local = glm::inverse(orientation_) * glm::vec3(0.0f, 1.0f, 0.0f);

        target_rot_velocity.x += world_up_in_local.z * kAutoLevelSpeed;

        float roll_correction = world_up_in_local.x * kAutoLevelSpeed;
        if (world_up_in_local.y < 0.0f) {
            roll_correction *= 3.0f;
            target_rot_velocity.x = 0;
            if (abs(world_up_in_local.x) < 0.1f) {
                roll_correction += kRollSpeed * 0.5f;
            }
        }
        target_rot_velocity.z -= roll_correction;
    }

    // --- Apply Damping and Update Rotational Velocity ---
    rotational_velocity_ += (target_rot_velocity - rotational_velocity_) * kDamping * delta_time;

    // --- Update Orientation ---
    glm::quat pitch_delta = glm::angleAxis(rotational_velocity_.x * delta_time, glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat yaw_delta = glm::angleAxis(rotational_velocity_.y * delta_time, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat roll_delta = glm::angleAxis(rotational_velocity_.z * delta_time, glm::vec3(0.0f, 0.0f, 1.0f));
    orientation_ = glm::normalize(orientation_ * pitch_delta * yaw_delta * roll_delta);

    // --- Update Speed (Boost & Decay) ---
    if (controller_->boost) {
        forward_speed_ += kBoostAcceleration * delta_time;
        if (forward_speed_ > kBoostSpeed)
            forward_speed_ = kBoostSpeed;
    } else {
        if (forward_speed_ > kBaseSpeed) {
            forward_speed_ -= kSpeedDecay * delta_time;
            if (forward_speed_ < kBaseSpeed)
                forward_speed_ = kBaseSpeed;
        }
    }

    // --- Update Velocity and Position ---
    glm::vec3 forward_dir = orientation_ * glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 new_velocity = forward_dir * forward_speed_;

    SetVelocity(Vector3(new_velocity.x, new_velocity.y, new_velocity.z));
}

void PaperPlane::UpdateShape() {
    Entity<Model>::UpdateShape();
    if (shape_) {
        shape_->SetRotation(orientation_);
    }
}

} // namespace Boidsish
