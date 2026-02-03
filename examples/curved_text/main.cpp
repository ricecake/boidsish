#include <iostream>
#include <memory>
#include <vector>

#include "graphics.h"
#include <GLFW/glfw3.h>

int main() {
	Boidsish::Visualizer visualizer(1280, 720, "Curved Text Effect Example");

	Boidsish::Camera camera;
	camera.x = 0.0f;
	camera.y = 20.0f;
	camera.z = 50.0f;
	camera.pitch = -15.0f;
	camera.yaw = 0.0f;
	visualizer.SetCamera(camera);

	// Add some light
	auto& light_manager = visualizer.GetLightManager();
	light_manager.AddLight(Boidsish::Light::CreateDirectional(
		glm::vec3(10.0f, 20.0f, 10.0f),
		glm::vec3(-1.0f, -1.0f, -1.0f),
		1.0f,
		glm::vec3(1.0f, 1.0f, 1.0f)
	));
	light_manager.SetAmbientLight(glm::vec3(0.3f));

	visualizer.AddInputCallback([&](const Boidsish::InputState& state) {
		if (state.mouse_button_down[GLFW_MOUSE_BUTTON_LEFT]) {
			auto world_pos = visualizer.ScreenToWorld(state.mouse_x, state.mouse_y);
			if (world_pos) {
				std::cout << "Spawning 'Can' style text at terrain" << std::endl;

				// Spawn curved text effect (Can style: text normal perp to wrap axis)
				visualizer.AddCurvedTextEffect(
					"+100 POINTS",
					*world_pos + glm::vec3(0, 5, 0), // center of cylinder
					10.0f,          // radius
					90.0f,          // angle
					glm::vec3(0, 1, 0), // wrap axis (up)
					glm::vec3(1, 0, 0), // text normal (midpoint facing)
					3.0f,           // duration
					"assets/Roboto-Medium.ttf",
					2.0f,           // font size
					0.5f,           // depth
					glm::vec3(1.0f, 1.0f, 0.0f) // yellow
				);
			}
		}

		if (state.key_down[GLFW_KEY_SPACE]) {
			std::cout << "Spawning 'Rainbow' style text" << std::endl;
			// Spawn curved text effect (Rainbow style: text normal parallel to wrap axis)
			visualizer.AddCurvedTextEffect(
				"DOUBLE RAINBOW",
				glm::vec3(0, 25, 0), // center of circle
				25.0f,             // radius
				180.0f,            // angle
				glm::vec3(0, 0, 1), // wrap axis (z)
				glm::vec3(0, 0, 1), // text normal (z - facing camera)
				5.0f,              // duration
				"assets/Roboto-Medium.ttf",
				3.0f,              // font size
				0.5f,              // depth
				glm::vec3(0.0f, 1.0f, 1.0f) // cyan
			);
		}
	});

	// Initial help message
	std::cout << "Controls:" << std::endl;
	std::cout << "  Left Click: Spawn curved text on terrain" << std::endl;
	std::cout << "  Space: Spawn multiple curved text effects around the center" << std::endl;
	std::cout << "  WASD: Move camera" << std::endl;
	std::cout << "  ESC: Exit" << std::endl;

	visualizer.Run();

	return 0;
}
