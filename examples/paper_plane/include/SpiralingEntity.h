#pragma once

#include "entity.h"
#include "model.h"
#include <glm/gtc/quaternion.hpp>
#include <random>

namespace Boidsish {

class SpiralingEntity : public Entity<Model> {
public:
    SpiralingEntity(int id = 0, Vector3 pos = {0, 0, 0});

    void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;
    void UpdateShape() override;

private:
    // Flight model
    glm::quat orientation_;
    glm::vec3 rotational_velocity_; // x: pitch, y: yaw, z: roll
    float forward_speed_;
    std::random_device rd_;
    std::mt19937 eng_;
    bool handedness_; // true for right, false for left

    enum class FlightState {
        Homing,
        Spiraling,
        Breaking,
        Looping
    };
    FlightState current_state_ = FlightState::Homing;
    float state_timer_ = 0.0f;
};

} // namespace Boidsish
