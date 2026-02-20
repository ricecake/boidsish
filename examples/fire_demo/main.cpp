#include <iostream>

#include "fire_effect.h"
#include "graphics.h"
#include "logger.h"
#include "shape.h"

int main() {
	try {
		Boidsish::Visualizer vis(1280, 720, "Fire Effect Demo");


		vis.GetCamera().y = 5.0;
		vis.GetCamera().z = 30.0;
		vis.SetCameraMode(Boidsish::CameraMode::STATIONARY);

		// Add a fire effect at the origin and store the pointer
		auto fire1 = vis.AddFireEffect(
			glm::vec3(-10.0f, 5.0f, 0.0f),
			Boidsish::FireEffectStyle::MissileExhaust,
			{0.01f, -1.0f, 0.0f}
		);
		auto fire3 = vis.AddFireEffect(glm::vec3(0.0f, 2.0f, 0.0f), Boidsish::FireEffectStyle::Fire);
		auto fire4 = vis.AddFireEffect(
			glm::vec3(0.0f, 10.0f, 10.0f),
			Boidsish::FireEffectStyle::Sparks,
			{0.01f, 1.0f, 0.0f},
			glm::vec3(0),
			20
		);

		float                                         boomTime = 0;
		std::vector<std::shared_ptr<Boidsish::Shape>> vec;
		vis.AddShapeHandler([&](float time) {
			if ((time - boomTime) > 5) {
				vis.CreateExplosion({5, 1, 5}, 2.0f);
				boomTime = time;
			}
			// // Dynamically update the fire effect's properties over time
			// float x = sin(time) * 5.0f;
			// float z = cos(time) * 5.0f;
			// fire_effect->SetPosition(glm::vec3(x, 3.0f, z));

			// Cycle through styles
			// int style_int = int(time / 2.0f) % 3;
			// fire_effect->SetStyle(static_cast<Boidsish::FireEffectStyle>(style_int));

			// // Make it point in a moving direction
			// fire_effect->SetDirection(glm::vec3(cos(time * 0.8f), sin(time * 0.8f), 1.0f));
			// fire1->SetDirection(glm::vec3(cos(time), sin(time), 1.0f));

			return vec;
		});

		vis.Run();
	} catch (const std::exception& e) {
		std::cerr << "An error occurred: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
