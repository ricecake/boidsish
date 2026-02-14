#include <cmath>
#include <cstdio>
#include <vector>

#include "graphics.h"
#include "hud.h"

// For key codes
#include <GLFW/glfw3.h>

using namespace Boidsish;

int main() {
	Visualizer viz(1024, 768, "HUD Demo");

	// Modern API: Add elements and keep pointers for easy updates
	// NOTE: This demo requires an image at "assets/icon.png".
	auto icon = viz.AddHudIcon("assets/icon.png", HudAlignment::TOP_LEFT, {10, 10}, {64, 64});
	auto timeDisplay = viz.AddHudNumber(0.0f, "Time", HudAlignment::TOP_RIGHT, {-10, 10}, 2);
	auto progressGauge = viz.AddHudGauge(0.0f, "Progress", HudAlignment::BOTTOM_CENTER, {0, -50}, {200, 20});

	// New widgets
	viz.AddHudCompass(HudAlignment::TOP_CENTER, {0, 20});
	viz.AddHudLocation(HudAlignment::BOTTOM_LEFT, {10, -10});
	auto scoreWidget = viz.AddHudScore(HudAlignment::TOP_RIGHT, {-10, 50});

	// Add an icon set (selectable)
	std::vector<std::string> weaponIcons =
		{"assets/missile-icon.png", "assets/bomb-icon.png", "assets/bullet-icon.png"};
	auto weaponSelector = viz.AddHudIconSet(weaponIcons, HudAlignment::TOP_LEFT, {10, 84}, {64, 64}, 10.0f);

	// Add an input handler
	viz.AddInputCallback([&](const InputState& state) {
		if (state.key_down[GLFW_KEY_H]) {
			icon->SetHighlighted(!icon->IsHighlighted());
		}
		if (state.key_down[GLFW_KEY_S]) {
			scoreWidget->AddScore(10, "Bonus!");
		}
		if (state.key_down[GLFW_KEY_F]) {
			int next = (weaponSelector->GetSelectedIndex() + 1) % 3;
			weaponSelector->SetSelectedIndex(next);
		}
	});

	// The main shape handler can still be used for frame updates
	viz.AddShapeHandler([&](float time) {
		timeDisplay->SetValue(time);

		float progress = fmod(time, 5.0f) / 5.0f;
		progressGauge->SetValue(progress);

		char overlay[16];
		snprintf(overlay, sizeof(overlay), "%.0f%%", progress * 100);
		progressGauge->SetLabel(overlay);

		return std::vector<std::shared_ptr<Shape>>();
	});

	viz.Run();
	return 0;
}
