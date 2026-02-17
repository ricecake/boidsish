#include <iostream>
#include <vector>

#include "graphics.h"
#include "terrain_generator_interface.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

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
			// Left click: Single Cylinder Hole
			if (input.mouse_button_down[0]) {
				auto pos = visualizer.ScreenToWorld(input.mouse_x, input.mouse_y);
				if (pos) {
					glm::quat orientation = glm::quat(glm::vec3(glm::radians(pitch), glm::radians(yaw), 0.0f));
					std::cout << "Creating cylinder hole at (" << pos->x << ", " << pos->z << ") with pitch " << pitch << " and yaw " << yaw << std::endl;
					visualizer.GetTerrain()->AddCylinderHole(*pos, 10.0f, 40.0f, orientation, false);
				}
			}
			// Right click: Tunneling logic
			else if (input.mouse_button_down[1]) {
				auto pos = visualizer.ScreenToWorld(input.mouse_x, input.mouse_y);
				if (pos) {
					glm::vec3 entry = *pos;
					glm::vec3 cam_front = visualizer.GetCamera().front();
					// Move slightly inside to avoid immediate exit
					glm::vec3 dir = glm::normalize(glm::vec3(cam_front.x, 0.0f, cam_front.z));

					float max_dist = 600.0f;
					float step = 2.0f;
					bool has_entered = false;
					glm::vec3 exit = entry + dir * 100.0f; // Default

					for (float d = step; d < max_dist; d += step) {
						glm::vec3 p = entry + dir * d;
						auto [h, n] = visualizer.GetTerrainPropertiesAtPoint(p.x, p.z);

						if (!has_entered) {
							if (h > entry.y + 1.0f) {
								has_entered = true;
							}
						} else {
							if (h < entry.y - 1.0f) {
								exit = glm::vec3(p.x, entry.y, p.z);
								break;
							}
						}
					}

					glm::vec3 center = (entry + exit) * 0.5f;
					float length = glm::distance(entry, exit);
					// Cylinder local axis is Y. Rotate +Y to tunnel direction.
					glm::quat orientation = glm::rotation(glm::vec3(0.0f, 1.0f, 0.0f), dir);

					std::cout << "Tunneling from " << entry.x << "," << entry.z << " to " << exit.x << "," << exit.z << " length " << length << std::endl;
					visualizer.GetTerrain()->AddCylinderHole(center, 12.0f, length + 2.0f, orientation, true);
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
		std::cout << "Left Click: Create single cylinder hole" << std::endl;
		std::cout << "Right Click: Tunnel through terrain feature" << std::endl;
		std::cout << "Arrow Keys: Adjust pitch/yaw for single holes" << std::endl;
		std::cout << "C Key: Clear all deformations" << std::endl;

		visualizer.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
