#pragma once

#include <random>
#include "entity.h"
#include "model.h"
#include "paper_plane.h"
#include "guided_missile.h"

namespace Boidsish {

class MissileLauncher: public Entity<Model> {
public:
    MissileLauncher(int id = 0, Vector3 pos = {0, 0, 0});

    void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;

private:
    float time_since_last_fire_ = 0.0f;
    float fire_interval_ = 5.0f; // Fire every 5 seconds, will be randomized

    std::random_device rd_;
    std::mt19937 eng_;
};

}
