#include <iostream>
#include <vector>

#include "graphics.h"
#include "terrain_generator_interface.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

int main() {
	try {
		Boidsish::Visualizer visualizer(1280, 720, "Cylinder Hole Demo");

		// Set a custom camera position
		Boidsish::Camera camera;
		camera.x = 0.0f;
		camera.y = 100.0f;
		camera.z = 200.0f;
		camera.pitch = -30.0f;
		camera.yaw = 0.0f;
		visualizer.SetCamera(camera);

		// Orientation for new cylinder holes
		static float pitch = 0.0f;
		static float yaw = 0.0f;

		// Add input callback for mouse interactions
		visualizer.AddInputCallback([&visualizer](const Boidsish::InputState& input) {
			// Left click: Cylinder Hole
			if (input.mouse_button_down[0]) {
				auto pos = visualizer.ScreenToWorld(input.mouse_x, input.mouse_y);
				if (pos) {
					glm::quat orientation = glm::quat(glm::vec3(glm::radians(pitch), glm::radians(yaw), 0.0f));
					std::cout << "Creating cylinder hole at (" << pos->x << ", " << pos->z << ") with pitch " << pitch << " and yaw " << yaw << std::endl;
					visualizer.GetTerrain()->AddCylinderHole(*pos, 10.0f, 40.0f, orientation);
				}
			}
			// Arrow keys: Adjust orientation for next hole
			else if (input.keys[GLFW_KEY_UP]) pitch += 1.0f;
			else if (input.keys[GLFW_KEY_DOWN]) pitch -= 1.0f;
			else if (input.keys[GLFW_KEY_LEFT]) yaw += 1.0f;
			else if (input.keys[GLFW_KEY_RIGHT]) yaw -= 1.0f;
			// C key: Clear deformations
			else if (input.key_down[GLFW_KEY_C]) {
				visualizer.GetTerrain()->GetDeformationManager().Clear();
				visualizer.GetTerrain()->InvalidateDeformedChunks();
			}
		});

		std::cout << "Cylinder Hole Demo" << std::endl;
		std::cout << "Left Click: Create cylinder hole with interior mesh" << std::endl;
		std::cout << "Arrow Keys: Adjust pitch/yaw for next cylinder hole" << std::endl;
		std::cout << "C Key: Clear all deformations" << std::endl;

		visualizer.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
