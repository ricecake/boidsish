#include <iostream>

#include "fire_effect.h"
#include "graphics.h"
#include "shape.h"

int main() {
	try {
		Boidsish::Visualizer vis(1280, 720, "Fire Effect Demo");

		// Add a fire effect at the origin and store the pointer
		auto fire_effect = vis.AddFireEffect(glm::vec3(0.0f, 3.0f, 0.0f), glm::vec3(0, 1, 0));

		vis.GetCamera().y = 5.0;
		vis.GetCamera().z = 20.0;
		vis.SetCameraMode(Boidsish::CameraMode::STATIONARY);

		std::vector<std::shared_ptr<Boidsish::Shape>> vec;
		vis.AddShapeHandler([&](float time) {
			// Dynamically update the fire effect's properties over time
			float x = sin(time) * 5.0f;
			float z = cos(time) * 5.0f;
			fire_effect->SetPosition(glm::vec3(x, 3.0f, z));

			// Cycle through styles
			int style = int(time / 2.0f) % 3;
			fire_effect->SetStyle(style);

			// Make it point upwards
			fire_effect->SetDirection(glm::vec3(sin(time), cos(time), 0));

			return vec;
		});

		vis.Run();
	} catch (const std::exception& e) {
		std::cerr << "An error occurred: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
