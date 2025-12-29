#pragma once

#include "IWidget.h"

namespace Boidsish {

class DebugLaser; // Forward declaration

namespace UI {

class DebugWidget : public IWidget {
public:
    DebugWidget(DebugLaser& laser_manager);
    void Draw() override;

private:
    DebugLaser& laser_manager_;
};

} // namespace UI
} // namespace Boidsish
