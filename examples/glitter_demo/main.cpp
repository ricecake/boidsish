#include <iostream>
#include <memory>
#include <vector>

#include "graphics.h"
#include <GLFW/glfw3.h>

using namespace Boidsish;

int main() {
	try {
		auto visualizer = std::make_shared<Visualizer>(1280, 720, "Glitter Demo");

		visualizer->AddInputCallback([&](const Boidsish::InputState& state) {
			if (state.key_down[GLFW_KEY_SPACE]) {
				visualizer->AddFireEffect(
					glm::vec3(0, 10, 0),
					FireEffectStyle::Glitter,
					glm::vec3(0, 1, 0), // direction (up)
					glm::vec3(0, 0, 0), // velocity
					-1,                 // max particles
					5.0f                // lifetime
				);
				std::cout << "Glitter effect spawned!" << std::endl;
			}

			if (state.key_down[GLFW_KEY_G]) {
				// Spawn glitter in a random direction
				float     angle = static_cast<float>(static_cast<double>(rand()) / static_cast<double>(RAND_MAX) * 2.0 * 3.1415926535);
				glm::vec3 dir(cos(angle), 1.0f, sin(angle));
				visualizer->AddFireEffect(
					glm::vec3(0, 5, 0),
					FireEffectStyle::Glitter,
					glm::normalize(dir),
					glm::vec3(0, 0, 0),
					500,
					3.0f
				);
			}
		});

		visualizer->Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
