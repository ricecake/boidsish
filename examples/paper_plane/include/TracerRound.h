#pragma once

#include "entity.h"

namespace Boidsish {

class TracerRound : public Entity<Dot> {
public:
    TracerRound(int id, Vector3 start_pos, Vector3 end_pos, bool is_tracer);
    void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;

private:
    Vector3 start_pos_;
    Vector3 end_pos_;
    float speed_ = 1000.0f;
    float lifetime_ = 0.0f;
    float max_lifetime_ = 2.0f; // rounds self-destruct after 2 seconds
};

} // namespace Boidsish
