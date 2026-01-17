#pragma once

#include "entity.h"
#include "model.h"

namespace Boidsish {
    class Bullet : public Entity<Model> {
    public:
        Bullet(int id, Vector3 pos, Vector3 vel);
        void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;

    private:
        float lifetime_ = 3.0f;
    };
}
