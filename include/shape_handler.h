#pragma once

#include <memory>
#include <vector>

#include "shape.h"
#include "visual_effects.h"

namespace Boidsish {

// Base class for handlers that manage and provide shapes for rendering
class ShapeHandler {
public:
    virtual ~ShapeHandler() = default;

    // Pure virtual function to be implemented by derived handlers
    virtual const std::vector<std::shared_ptr<Shape>>& GetShapes(float time) = 0;

    // Get a reference to the handler's effect set to apply effects to all its shapes
    EffectSet& GetEffectSet() { return effect_set_; }

protected:
    EffectSet effect_set_;
};

} // namespace Boidsish