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
				std::cout << "Spawning curved text at: " << world_pos->x << ", " << world_pos->y << ", " << world_pos->z
						  << std::endl;

				// Spawn curved text effect
				visualizer.AddCurvedTextEffect(
					"+100 POINTS",
					*world_pos + glm::vec3(0, 5, 0),
					10.0f,          // radius
					90.0f,          // angle
					glm::vec3(0, 1, 0), // normal (up)
					3.0f,           // duration
					"assets/Roboto-Medium.ttf",
					2.0f,           // font size
					0.5f,           // depth
					glm::vec3(1.0f, 1.0f, 0.0f) // yellow
				);
			}
		}

		if (state.key_down[GLFW_KEY_SPACE]) {
			// Spawn some examples around the center
			for (int i = 0; i < 8; ++i) {
				float angle = i * glm::two_pi<float>() / 8.0f;
				glm::vec3 pos(20.0f * cos(angle), 10.0f, 20.0f * sin(angle));
				glm::vec3 normal = glm::normalize(pos);

				visualizer.AddCurvedTextEffect(
					"BOIDSISH ENGINE",
					pos,
					15.0f,
					120.0f,
					normal,
					4.0f,
					"assets/Roboto-Medium.ttf",
					1.5f,
					0.2f
				);
			}
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
