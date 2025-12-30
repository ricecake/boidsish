#pragma once

#include "model.h"
#include "spatial_entity_handler.h"
#include <glm/gtc/quaternion.hpp>
#include <random>

namespace Boidsish {

class GuidedMissile : public Entity<Model> {
public:
    GuidedMissile(int id = 0, Vector3 pos = {0, 0, 0});

    void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;
    void UpdateShape() override;

private:
    constexpr static int thrust{50};
    constexpr static int lifetime{12};
    float lived = 0;
    bool exploded = false;

    // Flight model
    glm::quat orientation_;
    glm::vec3 rotational_velocity_; // x: pitch, y: yaw, z: roll
    float forward_speed_;
    std::random_device rd_;
    std::mt19937 eng_;
};

} // namespace Boidsish
