#include <cmath>
#include <cstdio>
#include <vector>

#include "graphics.h"
#include "hud.h"

// For key codes
#include <GLFW/glfw3.h>

using namespace Boidsish;

// Keep track of highlight state globally for simplicity in the demo
bool g_icon_highlighted = false;

int main() {
	Visualizer viz(1024, 768, "HUD Demo");

	// Add an icon. ID 1.
	// NOTE: This demo requires an image at "assets/icon.png".
	// If it's missing, an error will be logged, and the icon will not be displayed.
	viz.AddHudIcon({1, "assets/icon.png", HudAlignment::TOP_LEFT, {10, 10}, {64, 64}, g_icon_highlighted});

	// Add a number display. ID 2.
	viz.AddHudNumber({2, 0.0f, "Time", HudAlignment::TOP_RIGHT, {-10, 10}, 2});

	// Add a gauge. ID 3.
	viz.AddHudGauge({3, 0.0f, "0%", HudAlignment::BOTTOM_CENTER, {0, -50}, {200, 20}});

	// Add an input handler to toggle the icon's highlight state on 'H' press
	viz.AddInputCallback([&](const InputState& state) {
		if (state.key_down[GLFW_KEY_H]) {
			g_icon_highlighted = !g_icon_highlighted;
			// Update the icon with the new state. We must provide the full struct.
			viz.UpdateHudIcon(
				1,
				{1, "assets/icon.png", HudAlignment::TOP_LEFT, {10, 10}, {64, 64}, g_icon_highlighted}
			);
		}
	});

	// The main shape handler will update the dynamic HUD elements
	viz.AddShapeHandler([&](float time) {
		// Update number
		viz.UpdateHudNumber(2, {2, time, "Time", HudAlignment::TOP_RIGHT, {-10, 10}, 2});

		// Update gauge
		float progress = fmod(time, 5.0f) / 5.0f;
		char  overlay[10];
		snprintf(overlay, 10, "%.0f%%", progress * 100);
		viz.UpdateHudGauge(3, {3, progress, overlay, HudAlignment::BOTTOM_CENTER, {0, -50}, {200, 20}});

		// No 3D shapes needed for this demo
		return std::vector<std::shared_ptr<Shape>>();
	});

	viz.Run();
	return 0;
}
