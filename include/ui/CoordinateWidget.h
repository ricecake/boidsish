#pragma once

#include "IWidget.h"
#include <glm/glm.hpp>

namespace Boidsish {
namespace UI {

class CoordinateWidget : public IWidget {
public:
    CoordinateWidget() : _worldPos(0.0f) {}

    void Draw() override;
    void SetWorldPosition(const glm::vec3& pos) { _worldPos = pos; }

private:
    glm::vec3 _worldPos;
};

} // namespace UI
} // namespace Boidsish
