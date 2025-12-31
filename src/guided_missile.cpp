#include "guided_missile.h"
#include "entity.h"
#include <glm/gtc/quaternion.hpp>

namespace Boidsish {

GuidedMissile::GuidedMissile(int id, Vector3 pos):
    Entity<Model>(id, "assets/Missile.obj", true),
    rotational_velocity_(glm::vec3(0.0f)),
    forward_speed_(0.0f),
    eng_(rd_()) {
    SetPosition(pos.x, pos.y, pos.z);
    SetVelocity(0, 0, 0);
    SetTrailLength(500);
    SetTrailRocket(true);
    shape_->SetScale(glm::vec3(0.08f));
    std::dynamic_pointer_cast<Model>(shape_)->SetBaseRotation(
        glm::angleAxis(glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f))
    );
    UpdateShape();
}

void GuidedMissile::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
    lived += delta_time;
    if (lived >= lifetime) {
        handler.QueueRemoveEntity(id_);
        return;
    }
    if (exploded) {
        return;
    }

    // --- Flight Model Constants ---
    const float kLaunchTime = 0.5f;
    const float kMaxSpeed = 150.0f;
    const float kAcceleration = 150.0f;

    // --- Launch Phase ---
    if (lived < kLaunchTime) {
        // Set orientation to point straight up.
        // The model's "forward" is -Z, so we rotate it to point along +Y.
        orientation_ = glm::angleAxis(glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

        // Accelerate
        forward_speed_ += kAcceleration * delta_time;
        if (forward_speed_ > kMaxSpeed) {
            forward_speed_ = kMaxSpeed;
        }
    } else {
        // --- Guidance Phase ---
        const float kTurnSpeed = 4.0f;
        const float kDamping = 2.5f;

        auto targets = handler.GetEntitiesByType<PaperPlane>();
        if (targets.empty()) {
            // No target, fly straight.
            rotational_velocity_ = glm::vec3(0.0f);
        } else {
            auto plane = targets[0];

            // --- Proximity Detonation ---
            if ((plane->GetPosition() - GetPosition()).Magnitude() < 10) {
                SetVelocity(0, 0, 0);
                SetSize(100);
                SetColor(1, 0, 0, 0.33f);
                exploded = true;
                lived = -5; // Used for explosion lifetime
                return;
            }

            // --- Proportional Guidance ---
            // 1. Get world-space direction to target
            Vector3   target_vec = (plane->GetPosition() - GetPosition()).Normalized();
            glm::vec3 target_dir_world = glm::vec3(target_vec.x, target_vec.y, target_vec.z);

            // 2. Convert to missile's local space
            glm::vec3 target_dir_local = glm::inverse(orientation_) * target_dir_world;

            // 3. Calculate target rotational velocity
            //    The local target's X component drives yaw, Y component drives pitch.
            //    This creates a proportional control: the further off-axis the target is, the stronger the turn.
            glm::vec3 target_rot_velocity = glm::vec3(0.0f);
            target_rot_velocity.y = target_dir_local.x * kTurnSpeed;  // Yaw
            target_rot_velocity.x = -target_dir_local.y * kTurnSpeed; // Pitch

            // 4. Damp and apply rotational velocity
            rotational_velocity_ += (target_rot_velocity - rotational_velocity_) * kDamping * delta_time;

            if (lived <= 1.5f) {
                std::uniform_real_distribution<float> dist(-4.0f, 4.0f);
                glm::vec3                             error_vector(0.1f*dist(eng_), dist(eng_), 0);
                rotational_velocity_ += error_vector * delta_time;
            }
        }
    }

    // --- Update Orientation ---
    glm::quat pitch_delta = glm::angleAxis(rotational_velocity_.x * delta_time, glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat yaw_delta = glm::angleAxis(rotational_velocity_.y * delta_time, glm::vec3(0.0f, 1.0f, 0.0f));
    orientation_ = glm::normalize(orientation_ * pitch_delta * yaw_delta);

    // --- Update Velocity and Position ---
    glm::vec3 forward_dir = orientation_ * glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 new_velocity = forward_dir * forward_speed_;
    SetVelocity(Vector3(new_velocity.x, new_velocity.y, new_velocity.z));
}

void GuidedMissile::UpdateShape() {
    // First, call the base implementation
    Entity<Model>::UpdateShape();
    // Then, apply our specific orientation that includes roll
    if (shape_) {
        shape_->SetRotation(orientation_);
    }
}

}
