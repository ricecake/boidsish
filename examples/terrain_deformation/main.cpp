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
		camera.x = 0.0f;
		camera.y = 100.0f;
		camera.z = 200.0f;
		camera.pitch = -30.0f;
		camera.yaw = 0.0f;
		visualizer.SetCamera(camera);

		// Add input callback for mouse interactions
		visualizer.AddInputCallback([&visualizer](const Boidsish::InputState& input) {
			// Left click: Crater + Explosion
			if (input.mouse_button_down[0]) {
				auto pos = visualizer.ScreenToWorld(input.mouse_x, input.mouse_y);
				if (pos) {
					// Visual effect
					visualizer.CreateExplosion(*pos, 1.0f);

					// Terrain deformation
					visualizer.GetTerrain()->AddCrater(*pos, 15.0f, 8.0f, 0.2f, 2.0f);
				}
			}
			// Right click: Flatten
			else if (input.mouse_button_down[1]) {
				auto pos = visualizer.ScreenToWorld(input.mouse_x, input.mouse_y);
				if (pos) {
					// Terrain deformation
					visualizer.GetTerrain()->AddFlattenSquare(*pos, 20.0f, 20.0f, 5.0f, 0.0f);
				}
			}
			// K key: Akira effect
			else if (input.key_down[GLFW_KEY_K]) {
				auto pos = visualizer.ScreenToWorld(input.mouse_x, input.mouse_y);
				if (pos) {
					visualizer.TriggerAkira(*pos, 25.0f);
				}
			}
		});

		std::cout << "Terrain Deformation Demo" << std::endl;
		std::cout << "Left Click: Create crater and explosion" << std::endl;
		std::cout << "Right Click: Flatten terrain" << std::endl;
		std::cout << "K Key: Trigger Akira effect" << std::endl;

		visualizer.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
