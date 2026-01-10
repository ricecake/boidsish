#include <functional>
#include <iostream>

#include "GraphExample.h"
#include "VectorDemoHandler.h"
#include "fire_effect_manager.h"
#include "graphics.h"
#include <GLFW/glfw3.h>

using namespace Boidsish;

int main() {
	try {
		Boidsish::Visualizer viz(1200, 800, "Vector3 Operations Demo");

		// Set up camera
		Camera camera;
		camera.x = 0.0f;
		camera.y = 5.0f;
		camera.z = 15.0f;
		camera.yaw = 0.0f;
		camera.pitch = -15.0f;
		viz.SetCamera(camera);

		// Create and set the vector demo handler
		VectorDemoHandler handler(viz.GetThreadPool());
		viz.AddShapeHandler(std::ref(handler));
		viz.AddShapeHandler(std::ref(GraphExample));

		std::cout << "Vector Demo Started!" << std::endl;
		std::cout << "Controls:" << std::endl;
		std::cout << "  WASD - Move camera" << std::endl;
		std::cout << "  Space/Shift - Move up/down" << std::endl;
		std::cout << "  Mouse - Look around" << std::endl;
		std::cout << "  0 - Toggle auto-camera" << std::endl;
		std::cout << "  ESC - Exit" << std::endl;

		viz.AddInputCallback([&](const InputState& state) {
			if (state.key_down[GLFW_KEY_G]) {
				viz.TogglePostProcessingEffect("Film Grain");
			}
		});

		// Main loop
		auto fire_manager = viz.GetFireEffectManager();
		auto emitter1 = fire_manager->AddEffect(
			glm::vec3(0, 5, 0),
			FireEffectStyle::Fire,
			glm::vec3(0, 1, 0),
			glm::vec3(0, 0, 0),
			5000
		);
		auto emitter2 = fire_manager->AddEffect(
			glm::vec3(5, 5, 0),
			FireEffectStyle::Fire,
			glm::vec3(0, 1, 0),
			glm::vec3(0, 0, 0),
			10000
		);
		auto emitter3 = fire_manager->AddEffect(
			glm::vec3(-5, 5, 0),
			FireEffectStyle::Fire,
			glm::vec3(0, 1, 0),
			glm::vec3(0, 0, 0),
			25
		);

		float start_time = 0.0f;
		bool  emitter_removed = false;
		while (!viz.ShouldClose()) {
			viz.Update();
			viz.Render();
			start_time += viz.GetLastFrameTime();
			if (start_time > 5.0f && !emitter_removed) {
				fire_manager->RemoveEffect(emitter2);
				emitter_removed = true;
			}
		}

		std::cout << "Vector demo ended." << std::endl;

	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
