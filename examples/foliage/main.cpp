#include <iostream>
#include <memory>
#include <vector>

#include "decor_manager.h"
#include "graphics.h"
#include <GLFW/glfw3.h>

using namespace Boidsish;

int main() {
	try {
		auto visualizer = std::make_shared<Visualizer>(1280, 720, "Glitter Demo");

		auto decor = visualizer->GetDecorManager();

		DecorProperties teapot_props;
		teapot_props.min_height = 0.01;
		teapot_props.max_height = 95.0f;
		teapot_props.min_density = 0.1f;
		teapot_props.max_density = 0.11f;
		teapot_props.base_scale = 0.008f;
		teapot_props.scale_variance = 0.01f;
		teapot_props.align_to_terrain = true; // Align to slope
		decor->AddDecorType("assets/tree01.obj", teapot_props);

		DecorProperties missile_props;
		teapot_props.min_height = 50.0f;
		teapot_props.max_height = 400.0f;
		missile_props.min_density = 0.001f;
		missile_props.max_density = 0.04f;
		missile_props.base_scale = 0.01f;
		missile_props.scale_variance = 0.005f;
		// missile_props.base_rotation = glm::vec3(-90.0f, 0.0f, 0.0f); // Point upward (rotate -90 on X)
		missile_props.random_yaw = true;
		missile_props.align_to_terrain = false; // Stay upright (default)
		decor->AddDecorType("assets/PUSHILIN_dead_tree.obj", missile_props);

		// visualizer->AddInputCallback([&](const Boidsish::InputState& state) {
		// 	if (state.key_down[GLFW_KEY_SPACE]) {
		// 		visualizer->AddFireEffect(
		// 			glm::vec3(0, 10, 0),
		// 			FireEffectStyle::Glitter,
		// 			glm::vec3(0, 1, 0), // direction (up)
		// 			glm::vec3(0, 0, 0), // velocity
		// 			-1,                 // max particles
		// 			5.0f                // lifetime
		// 		);
		// 		std::cout << "Glitter effect spawned!" << std::endl;
		// 	}

		// 	if (state.key_down[GLFW_KEY_G]) {
		// 		// Spawn glitter in a random direction
		// 		float angle = static_cast<float>(
		// 			static_cast<double>(rand()) / static_cast<double>(RAND_MAX) * 2.0 * 3.1415926535
		// 		);
		// 		glm::vec3 dir(cos(angle), 1.0f, sin(angle));
		// 		visualizer->AddFireEffect(
		// 			glm::vec3(0, 5, 0),
		// 			FireEffectStyle::Glitter,
		// 			glm::normalize(dir),
		// 			glm::vec3(0, 0, 0),
		// 			500,
		// 			3.0f
		// 		);
		// 	}
		// });

		visualizer->Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
