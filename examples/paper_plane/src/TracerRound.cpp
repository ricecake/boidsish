#include "TracerRound.h"

namespace Boidsish {

TracerRound::TracerRound(int id, Vector3 start_pos, Vector3 end_pos, bool is_tracer)
    : Entity<Dot>(id), start_pos_(start_pos), end_pos_(end_pos) {
    SetPosition(start_pos_);
    Vector3 direction = (end_pos_ - start_pos_).Normalized();
    SetVelocity(direction * speed_);
    if (is_tracer) {
        SetTrailLength(20);
        SetColor(1.0f, 0.8f, 0.2f); // Yellowish-orange tracer
    } else {
        SetTrailLength(0);
        SetColor(0.5f, 0.5f, 0.5f); // Dim grey for non-tracers
    }
    SetSize(2.0f);
}

void TracerRound::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
    lifetime_ += delta_time;
    if (lifetime_ > max_lifetime_) {
        handler.QueueRemoveEntity(GetId());
    }

    // Update position based on velocity
    SetPosition(GetPosition() + GetVelocity() * delta_time);
}

} // namespace Boidsish
