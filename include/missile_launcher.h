#pragma once

#include "model.h"
#include "spatial_entity_handler.h"
#include <random>

namespace Boidsish {

class MissileLauncher : public Entity<Model> {
public:
    MissileLauncher(int id, glm::vec3 position);

    void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;

private:
    float cooldown_;
    std::random_device rd_;
    std::mt19937 eng_;
    std::uniform_real_distribution<float> dist_;
    constexpr static float kFiringCooldown_ = 3.0f; // 3 seconds
};

} // namespace Boidsish
