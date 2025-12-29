#include "ui/DebugWidget.h"
#include "debug_laser.h"
#include "imgui.h"

namespace Boidsish {
namespace UI {

DebugWidget::DebugWidget(DebugLaser& laser_manager)
    : laser_manager_(laser_manager) {}

void DebugWidget::Draw() {
    if (ImGui::CollapsingHeader("Debug Tools")) {
        ImGui::Checkbox("Show Terrain Laser", &laser_manager_.is_enabled_);
    }
}

} // namespace UI
} // namespace Boidsish
