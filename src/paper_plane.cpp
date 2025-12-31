#include "paper_plane.h"
#include "entity.h"
#include <glm/gtc/quaternion.hpp>

namespace Boidsish {

PaperPlane::PaperPlane(int id):
    Entity<Model>(id, "assets/Mesh_Cat.obj", true),
    orientation_(glm::quat(1.0f, 0.0f, 0.0f, 0.0f)),
    rotational_velocity_(glm::vec3(0.0f)),
    forward_speed_(20.0f) {
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

void PaperPlane::SetController(std::shared_ptr<PaperPlaneInputController> controller) { controller_ = controller; }

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
    if (!controller_->pitch_up && !controller_->pitch_down && !controller_->yaw_left && !controller_->yaw_right &&
        !controller_->roll_left && !controller_->roll_right) {
        // This robust auto-leveling logic finds the shortest rotational path to bring the plane upright and level.
        // It works by finding where the world's 'up' vector is in relation to the plane's local axes,
        // and then applying corrective forces.

        glm::vec3 world_up_in_local = glm::inverse(orientation_) * glm::vec3(0.0f, 1.0f, 0.0f);

        // --- Pitch Correction (Shortest Path) ---
        // The 'z' component of world_up_in_local tells us how 'forward' or 'backward' the world's 'up' is.
        // A positive 'z' means world 'up' is in front of our nose (i.e., we are pitched down).
        // To correct, we pitch up (positive x rotation), correctly taking the shortest path to the horizon.
        target_rot_velocity.x += world_up_in_local.z * kAutoLevelSpeed;

        // --- Roll Correction ---
        // The 'x' component tells us how 'right' or 'left' the world's 'up' is.
        // A positive 'x' means world 'up' is to our right. We must roll right (negative z rotation) to level the
        // wings.
        float roll_correction = world_up_in_local.x * kAutoLevelSpeed;

        // The 'y' component tells us if we are upright or inverted. If it's negative, we are upside down.
        if (world_up_in_local.y < 0.0f) {
            roll_correction *= 3.0f; // Apply a stronger roll correction.
            target_rot_velocity.x = 0;

            // This solves the "stuck upside down" problem. If we're perfectly inverted, the roll_correction
            // can be zero. Here, we add a constant roll "kick" to get the plane rolling.
            if (abs(world_up_in_local.x) < 0.1f) {
                roll_correction += kRollSpeed * 0.5f;
            }
        }

        target_rot_velocity.z -= roll_correction;
    }

    // --- Apply Damping and Update Rotational Velocity ---
    // Lerp towards the target velocity to create a smooth, responsive feel
    rotational_velocity_ += (target_rot_velocity - rotational_velocity_) * kDamping * delta_time;

    // --- Update Orientation ---
    // Create delta rotations for pitch, yaw, and roll in the plane's local space.
    glm::quat pitch_delta = glm::angleAxis(rotational_velocity_.x * delta_time, glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat yaw_delta = glm::angleAxis(rotational_velocity_.y * delta_time, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat roll_delta = glm::angleAxis(rotational_velocity_.z * delta_time, glm::vec3(0.0f, 0.0f, 1.0f));

    // Combine the deltas and apply to the main orientation (post-multiplication for local-space rotation)
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
    // The model's "forward" is along the negative Z-axis in its local space
    glm::vec3 forward_dir = orientation_ * glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 new_velocity = forward_dir * forward_speed_;

    SetVelocity(Vector3(new_velocity.x, new_velocity.y, new_velocity.z));
}

void PaperPlane::UpdateShape() {
    // First, call the base implementation
    Entity<Model>::UpdateShape();
    // Then, apply our specific orientation that includes roll
    if (shape_) {
        shape_->SetRotation(orientation_);
    }
}

}
