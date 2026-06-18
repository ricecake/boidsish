#include <iostream>
#include "fire_effect.h"
#include "graphics.h"
#include "logger.h"

int main() {
	try {
		Boidsish::Visualizer vis(1280, 720, "Firefly Effect Test");

		vis.GetCamera().y = 5.0;
		vis.GetCamera().z = 20.0;
		vis.SetCameraMode(Boidsish::CameraMode::STATIONARY);

		// Add fireflies at the origin
		auto fireflies = vis.AddFireEffect(
			glm::vec3(0.0f, 5.0f, 0.0f),
			Boidsish::FireEffectStyle::Fireflies,
			{0.0f, 1.0f, 0.0f},
			glm::vec3(0),
			100, // max particles
            -1.0f, // lifetime
            Boidsish::EmitterType::Sphere,
            glm::vec3(5.0f) // dimensions
		);

		vis.Run();
	} catch (const std::exception& e) {
		std::cerr << "An error occurred: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
