#pragma once

#include "entity.h"
#include "laser.h"
#include <memory>

namespace Boidsish {

class PointDefenseCannon {
public:
    PointDefenseCannon(std::shared_ptr<EntityBase> parent);

    void Update(const EntityHandler& handler, float delta_time, bool should_fire);
    void SetTarget(std::shared_ptr<EntityBase> target);

    std::shared_ptr<Laser> GetLaser() const { return laser_; }

private:
    std::weak_ptr<EntityBase> parent_;
    std::weak_ptr<EntityBase> target_;
    std::shared_ptr<Laser> laser_;
    float fire_rate_ = 10.0f; // rounds per second
    float time_since_last_shot_ = 0.0f;
    int rounds_fired_ = 0;
};

} // namespace Boidsish
