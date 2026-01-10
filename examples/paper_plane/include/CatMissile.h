#pragma once

#include "SeekingMissile.h"
#include "PaperPlane.h"

namespace Boidsish {

class CatMissile : public SeekingMissile<PaperPlane, CatMissileFlightParams> {
public:
    CatMissile(
        int id = 0,
        Vector3 pos = {0, 0, 0},
        glm::quat orientation = {0, {0, 0, 0}},
        glm::vec3 dir = {0, 0, 0},
        Vector3 vel = {0, 0, 0}
    );

    void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;

private:
    bool fired_ = false;
};

} // namespace Boidsish
