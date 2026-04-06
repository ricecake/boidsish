#include <iostream>
#include <memory>
#include <vector>

#include "JetPlane.h"
#include "JetHandler.h"
#include "JetInputController.h"
#include "constants.h"
#include "graphics.h"
#include "hud.h"
#include "terrain_generator_interface.h"
#include <GLFW/glfw3.h>

using namespace Boidsish;

int main() {
	try {
		auto visualizer = std::make_shared<Visualizer>(
			Constants::Project::Window::DefaultWidth(),
			Constants::Project::Window::DefaultHeight(),
			"JetTimeMoFo"
		);

		auto terrain = visualizer->GetTerrain();
		terrain->SetWorldScale(4.0f);

		auto handler = std::make_shared<JetHandler>(visualizer->GetThreadPool());
		handler->SetVisualizer(visualizer);

		auto id = handler->AddEntity<JetPlane>();
		auto plane = std::dynamic_pointer_cast<JetPlane>(handler->GetEntity(id));

		handler->PreparePlane(plane);

		visualizer->AddShapeHandler(std::ref(*handler));

		auto throttleGauge = visualizer->AddHudGauge(1.0f, "Throttle", HudAlignment::BOTTOM_LEFT, {50, -50}, {200, 20});
		auto stabMessage = visualizer->AddHudMessage("Stabilization: ON", HudAlignment::TOP_CENTER, {0, 20}, 1.0f);

		auto controller = std::make_shared<JetInputController>();
		plane->SetController(controller);

		visualizer->AddInputCallback([&](const Boidsish::InputState& state) {
			controller->pitch_up = state.keys[GLFW_KEY_S];
			controller->pitch_down = state.keys[GLFW_KEY_W];
			controller->yaw_left = state.keys[GLFW_KEY_A];
			controller->yaw_right = state.keys[GLFW_KEY_D];
			controller->roll_left = state.keys[GLFW_KEY_Q];
			controller->roll_right = state.keys[GLFW_KEY_E];
			controller->throttle_up = state.keys[GLFW_KEY_U];
			controller->throttle_down = state.keys[GLFW_KEY_N];
			if (state.key_down[GLFW_KEY_T]) {
				controller->toggle_stabilization = true;
			}

			// Update HUD
			throttleGauge->SetValue(plane->GetThrottle());
			stabMessage->SetMessage(std::string("Stabilization: ") + (plane->IsStabilizationEnabled() ? "ON" : "OFF"));
		});

		visualizer->SetChaseCamera(plane);

		visualizer->Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
