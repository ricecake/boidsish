#pragma once

#include "entity.h"
#include "model.h"
#include "PaperPlane.h" // Default target
#include "terrain_generator.h"
#include "fire_effect.h"
#include "graphics.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <random>

namespace Boidsish {

// --- Flight Parameter Structs ---
struct GuidedMissileFlightParams {
    static constexpr float lifetime_ = 12.0f;
    static constexpr float kExplosionDisplayTime = 2.0f;
    static constexpr float kLaunchTime = 0.5f;
    static constexpr float kMaxSpeed = 170.0f;
    static constexpr float kAcceleration = 150.0f;
    static constexpr float kTurnSpeed = 4.0f;
    static constexpr float kDamping = 2.5f;
};

struct CatMissileFlightParams {
    static constexpr float lifetime_ = 12.0f;
    static constexpr float kExplosionDisplayTime = 2.0f;
    static constexpr float kLaunchTime = 1.0f;
    static constexpr float kMaxSpeed = 150.0f;
    static constexpr float kAcceleration = 150.0f;
    static constexpr float kTurnSpeed = 4.0f;
    static constexpr float kDamping = 2.5f;
};


template <class TargetEntityType, class FlightParams>
class SeekingMissile : public Entity<Model> {
public:
    SeekingMissile(int id, Vector3 pos, const std::string& model_path = "assets/Missile.obj")
        : Entity<Model>(id, model_path, true),
          rotational_velocity_(glm::vec3(0.0f)),
          forward_speed_(0.0f),
          eng_(rd_()) {
        this->SetPosition(pos.x, pos.y, pos.z);
        this->SetVelocity(0, 0, 0);
        this->SetTrailLength(500);
        this->SetTrailRocket(true);
        this->shape_->SetScale(glm::vec3(0.08f));
        std::dynamic_pointer_cast<Model>(this->shape_)->SetBaseRotation(
            glm::angleAxis(glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f))
        );
        UpdateShape();
    }

    void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override {
        lived_ += delta_time;
        auto pos = this->GetPosition();

        if (exploded_) {
            if (lived_ >= FlightParams::kExplosionDisplayTime) {
                handler.QueueRemoveEntity(this->id_);
            }
            return;
        }

        if (lived_ >= FlightParams::lifetime_) {
            Explode(handler, false);
            return;
        }

        if (lived_ < FlightParams::kLaunchTime) {
            orientation_ = glm::angleAxis(glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            forward_speed_ += FlightParams::kAcceleration * delta_time;
            if (forward_speed_ > FlightParams::kMaxSpeed) {
                forward_speed_ = FlightParams::kMaxSpeed;
            }
        } else {
            auto targets = handler.GetEntitiesByType<TargetEntityType>();
            if (targets.empty()) {
                rotational_velocity_ = glm::vec3(0.0f);
            } else {
                auto target = targets[0]; // Simplification: always target the first one

                if ((target->GetPosition() - this->GetPosition()).Magnitude() < 10) {
                    Explode(handler, true);
                    // A way to trigger damage on any entity type would be needed for a fully generic solution.
                    // For now, we assume a compatible interface or specialize.
                    if constexpr (std::is_same_v<TargetEntityType, PaperPlane>) {
                        target->TriggerDamage();
                    }
                    return;
                }

                Vector3   target_vec = (target->GetPosition() - this->GetPosition()).Normalized();
                glm::vec3 target_dir_world = glm::vec3(target_vec.x, target_vec.y, target_vec.z);
                glm::vec3 target_dir_local = glm::inverse(orientation_) * target_dir_world;

                glm::vec3 target_rot_velocity = glm::vec3(0.0f);
                target_rot_velocity.y = target_dir_local.x * FlightParams::kTurnSpeed;
                target_rot_velocity.x = -target_dir_local.y * FlightParams::kTurnSpeed;

                rotational_velocity_ += (target_rot_velocity - rotational_velocity_) * FlightParams::kDamping * delta_time;

                if (lived_ <= 1.5f) {
                    std::uniform_real_distribution<float> dist(-4.0f, 4.0f);
                    glm::vec3 error_vector(0.1f * dist(eng_), dist(eng_), 0);
                    rotational_velocity_ += error_vector * delta_time;
                }

                const auto* terrain_generator = handler.GetTerrainGenerator();
                if (terrain_generator) {
                    const float reaction_distance = 100.0f;
                    float       hit_dist = 0.0f;

                    Vector3 vel_vec = this->GetVelocity();
                    if (vel_vec.MagnitudeSquared() > 1e-6) {
                        glm::vec3 origin = {this->GetPosition().x, this->GetPosition().y, this->GetPosition().z};
                        glm::vec3 dir = {vel_vec.x, vel_vec.y, vel_vec.z};
                        dir = glm::normalize(dir);

                        if (terrain_generator->Raycast(origin, dir, reaction_distance, hit_dist)) {
                            auto hit_coord = vel_vec.Normalized() * hit_dist;
                            auto [terrain_h, terrain_normal] = terrain_generator->pointProperties(
                                hit_coord.x,
                                hit_coord.z
                            );

                            const float avoidance_strength = 20.0f;
                            const float kUpAlignmentThreshold = 0.5f;
                            float force_magnitude = avoidance_strength * (1.0f - ((10 + hit_dist) / reaction_distance));

                            glm::vec3 local_up = glm::vec3(0.0f, 1.0f, 0.0f);
                            auto      away = terrain_normal;
                            if (glm::dot(away, local_up) < kUpAlignmentThreshold) {
                                away = local_up;
                            }
                            glm::vec3 avoidance_force = away * force_magnitude;
                            glm::vec3 avoidance_local = glm::inverse(orientation_) * avoidance_force;
                            rotational_velocity_.y += avoidance_local.x * avoidance_strength * delta_time;
                            rotational_velocity_.x += avoidance_local.y * avoidance_strength * delta_time;
                        }
                    }
                }
            }
        }

        glm::quat pitch_delta = glm::angleAxis(rotational_velocity_.x * delta_time, glm::vec3(1.0f, 0.0f, 0.0f));
        glm::quat yaw_delta = glm::angleAxis(rotational_velocity_.y * delta_time, glm::vec3(0.0f, 1.0f, 0.0f));
        orientation_ = glm::normalize(orientation_ * pitch_delta * yaw_delta);

        glm::vec3 forward_dir = orientation_ * glm::vec3(0.0f, 0.0f, 1.0f);
        glm::vec3 new_velocity = forward_dir * forward_speed_;
        this->SetVelocity(Vector3(new_velocity.x, new_velocity.y, new_velocity.z));
    }

    void UpdateShape() override {
        Entity<Model>::UpdateShape();
        if (this->shape_) {
            this->shape_->SetRotation(orientation_);
        }
    }

    void Explode(const EntityHandler& handler, bool hit_target) {
        if (exploded_)
            return;

        auto pos = this->GetPosition();
        handler.EnqueueVisualizerAction([=, &handler]() {
            handler.vis->AddFireEffect(
                glm::vec3(pos.x, pos.y, pos.z),
                FireEffectStyle::Explosion,
                glm::vec3(0, 1, 0),
                glm::vec3(0, 0, 0),
                -1,
                2.0f
            );
        });

        handler.EnqueueVisualizerAction([exhaust = exhaust_effect_]() {
            if (exhaust) {
                exhaust->SetLifetime(0.25f);
                exhaust->SetLived(0.0f);
            }
        });

        exploded_ = true;
        lived_ = 0.0f;
        this->SetVelocity(Vector3(0, 0, 0));

        if (hit_target) {
            this->SetSize(100);
            this->SetColor(1, 0, 0, 0.33f);
        }
    }

protected:
    float                       lived_ = 0.0f;
    bool                        exploded_ = false;
    std::shared_ptr<FireEffect> exhaust_effect_ = nullptr;

    // Flight model
    glm::quat          orientation_;
    glm::vec3          rotational_velocity_; // x: pitch, y: yaw, z: roll
    float              forward_speed_;
    std::random_device rd_;
    std::mt19937       eng_;
};

} // namespace Boidsish
