#pragma once

#include "dot.h"
#include "visual_effects.h"
#include <vector>

namespace Boidsish {

// A new shape type that has the clone effect enabled.
class CloneableDot : public Dot {
public:
    // Inherit constructors from Dot
    using Dot::Dot;

    std::vector<VisualEffect> GetActiveEffects() const override {
        return {VisualEffect::FREEZE_FRAME_TRAIL};
    }
};

} // namespace Boidsish
