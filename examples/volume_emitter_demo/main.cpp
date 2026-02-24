#include <iostream>
#include "fire_effect.h"
#include "graphics.h"
#include "logger.h"
#include "shape.h"

int main() {
	try {
		Boidsish::Visualizer vis(1280, 720, "Volume Emitter Demo");

		vis.GetCamera().y = 20.0;
		vis.GetCamera().z = 60.0;
		vis.SetCameraMode(Boidsish::CameraMode::STATIONARY);

		// 1. Box Emitter with Bubbles
		auto box_bubbles = vis.AddFireEffect(
			glm::vec3(-20.0f, 10.0f, 0.0f),
			Boidsish::FireEffectStyle::Bubbles,
			glm::vec3(0, 1, 0),    // direction
			glm::vec3(0, 0, 0),    // velocity
			-1,                    // max particles
			-1.0f,                 // lifetime
			Boidsish::EmitterType::Box,
			glm::vec3(10.0f, 10.0f, 10.0f) // dimensions
		);

		// 2. Sphere Emitter with Fireflies
		auto sphere_fireflies = vis.AddFireEffect(
			glm::vec3(0.0f, 10.0f, 0.0f),
			Boidsish::FireEffectStyle::Fireflies,
			glm::vec3(0, 1, 0),
			glm::vec3(0, 0, 0),
			-1,
			-1.0f,
			Boidsish::EmitterType::Sphere,
			glm::vec3(8.0f, 0, 0) // radius in x
		);

		// 3. Beam Emitter with Fire
		auto beam_fire = vis.AddFireEffect(
			glm::vec3(20.0f, 2.0f, 0.0f),
			Boidsish::FireEffectStyle::Fire,
			glm::vec3(0, 1, 0), // Upward beam
			glm::vec3(0, 0, 0),
			-1,
			-1.0f,
			Boidsish::EmitterType::Beam,
			glm::vec3(15.0f, 0, 0) // length in x
		);

		// 4. Moving Box with Sparks
		auto moving_box = vis.AddFireEffect(
			glm::vec3(0.0f, 2.0f, 20.0f),
			Boidsish::FireEffectStyle::Sparks,
			glm::vec3(0, 1, 0),
			glm::vec3(0, 0, 0),
			-1,
			-1.0f,
			Boidsish::EmitterType::Box,
			glm::vec3(5.0f, 1.0f, 5.0f)
		);

		std::vector<std::shared_ptr<Boidsish::Shape>> vec;
		vis.AddShapeHandler([&](float time) {
			// Animate the moving box
			float x = sin(time) * 15.0f;
			if (moving_box) {
				moving_box->SetPosition(glm::vec3(x, 2.0f, 20.0f));
			}

			// Rotate the beam
			if (beam_fire) {
				beam_fire->SetDirection(glm::vec3(sin(time), cos(time), 0));
			}

			return vec;
		});

		vis.Run();
	} catch (const std::exception& e) {
		std::cerr << "An error occurred: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
