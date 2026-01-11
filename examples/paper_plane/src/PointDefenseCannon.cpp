#include "PointDefenseCannon.h"
#include "TracerRound.h"

namespace Boidsish {

PointDefenseCannon::PointDefenseCannon(std::shared_ptr<EntityBase> parent)
    : parent_(parent) {
    laser_ = std::make_shared<Laser>();
}

void PointDefenseCannon::SetTarget(std::shared_ptr<EntityBase> target) {
    target_ = target;
}

void PointDefenseCannon::Update(const EntityHandler& handler, float delta_time, bool should_fire) {
    auto parent = parent_.lock();
    if (!parent) return;

    auto target = target_.lock();
    if (target) {
        laser_->SetPoints(parent->GetPosition().Toglm(), target->GetPosition().Toglm());
    } else {
        // If there's no target, just point the laser forward
        glm::vec3 end_point = parent->GetPosition().Toglm() + parent->ObjectToWorld(glm::vec3(0.0f, 0.0f, -1000.0f));
        laser_->SetPoints(parent->GetPosition().Toglm(), end_point);
    }

    time_since_last_shot_ += delta_time;
    if (should_fire && target && time_since_last_shot_ >= 1.0f / fire_rate_) {
        time_since_last_shot_ = 0.0f;
        rounds_fired_++;

        bool is_tracer = (rounds_fired_ % 5 == 0);

        handler.QueueAddEntity<TracerRound>(parent->GetPosition(), target->GetPosition(), is_tracer);
    }
}

} // namespace Boidsish
