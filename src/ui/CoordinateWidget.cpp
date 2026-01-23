#include "ui/CoordinateWidget.h"
#include "imgui.h"

namespace Boidsish {
namespace UI {

void CoordinateWidget::Draw() {
    ImGui::Begin("World Coordinates");
    ImGui::Text("Clicked Position:");
    ImGui::Text("X: %.2f, Y: %.2f, Z: %.2f", _worldPos.x, _worldPos.y, _worldPos.z);
    ImGui::End();
}

} // namespace UI
} // namespace Boidsish
