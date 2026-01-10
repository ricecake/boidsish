#pragma once

#include "SeekingMissile.h"
#include "PaperPlane.h"

namespace Boidsish {

class GuidedMissile : public SeekingMissile<PaperPlane, GuidedMissileFlightParams> {
public:
    GuidedMissile(int id = 0, Vector3 pos = {0, 0, 0});
};

} // namespace Boidsish
