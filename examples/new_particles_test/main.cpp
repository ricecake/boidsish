#include <iostream>
#include <vector>
#include <memory>

#include "fire_effect.h"
#include "graphics.h"
#include "logger.h"
#include "shape.h"

int main() {
	try {
		Boidsish::Visualizer vis(1280, 720, "New Particles Test");

		vis.GetCamera().y = 10.0;
		vis.GetCamera().z = 30.0;
		vis.SetCameraMode(Boidsish::CameraMode::STATIONARY);

		// Add Debug effect
		auto debug_effect = vis.AddFireEffect(
			glm::vec3(-10.0f, 5.0f, 0.0f),
			Boidsish::FireEffectStyle::Debug,
			glm::vec3(0.0f),
			glm::vec3(0.0f),
			50 // max particles
		);

		// Add Cinder effect
		auto cinder_effect = vis.AddFireEffect(
			glm::vec3(10.0f, 2.0f, 0.0f),
			Boidsish::FireEffectStyle::Cinder,
			glm::vec3(0.0f, 1.0f, 0.0f),
			glm::vec3(0.0f),
			200 // max particles
		);

        // Add a Box emitter for Cinders to see them better
        auto cinder_box = vis.AddFireEffect(
            glm::vec3(0.0f, 2.0f, 10.0f),
            Boidsish::FireEffectStyle::Cinder,
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f),
            500,
            -1.0f,
            Boidsish::EmitterType::Box,
            glm::vec3(10.0f, 1.0f, 2.0f)
        );

		std::vector<std::shared_ptr<Boidsish::Shape>> vec;
		vis.AddShapeHandler([&](float time) {
			return vec;
		});

		vis.Run();
	} catch (const std::exception& e) {
		std::cerr << "An error occurred: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
