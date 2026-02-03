#include <iostream>
#include <vector>

#include "graphics.h"
#include "terrain_generator.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

int main() {
	try {
		Boidsish::Visualizer visualizer(1280, 720, "Terrain Deformation Demo");

		// Set a custom camera position
		Boidsish::Camera camera;
		camera.x = 50.0f;
		camera.y = 100.0f;
		camera.z = 50.0f;
		camera.pitch = -45.0f;
		camera.yaw = -135.0f;
		visualizer.SetCamera(camera);

		visualizer.AddInputCallback([&visualizer](const Boidsish::InputState& state) {
			if (state.mouse_button_down[GLFW_MOUSE_BUTTON_LEFT]) {
				auto pos_opt = visualizer.ScreenToWorld(state.mouse_x, state.mouse_y);
				if (pos_opt) {
					glm::vec3 pos = *pos_opt;
					std::cout << "Left Click at: " << pos.x << ", " << pos.y << ", " << pos.z << std::endl;
					visualizer.CreateExplosion(pos, 1.0f);
					visualizer.AddTerrainDeformation(pos, 8.0f, 4.0f, Boidsish::DeformationType::CRATER);
				}
			}
			if (state.mouse_button_down[GLFW_MOUSE_BUTTON_RIGHT]) {
				auto pos_opt = visualizer.ScreenToWorld(state.mouse_x, state.mouse_y);
				if (pos_opt) {
					glm::vec3 pos = *pos_opt;
					std::cout << "Right Click at: " << pos.x << ", " << pos.y << ", " << pos.z << std::endl;
					visualizer.AddTerrainDeformation(pos, 12.0f, pos.y, Boidsish::DeformationType::FLATTEN);
				}
			}
			if (state.key_down[GLFW_KEY_C]) {
				std::cout << "Clearing deformations" << std::endl;
				visualizer.ClearTerrainDeformations();
			}
		});

		std::cout << "Controls:" << std::endl;
		std::cout << "  Left Click: Create Explosion and Crater" << std::endl;
		std::cout << "  Right Click: Flatten Terrain" << std::endl;
		std::cout << "  C: Clear all deformations" << std::endl;
		std::cout << "  WASD: Move camera (Free mode)" << std::endl;
		std::cout << "  0: Switch to Free camera mode" << std::endl;

		visualizer.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
