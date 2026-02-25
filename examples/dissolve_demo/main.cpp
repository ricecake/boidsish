#include <iostream>
#include <vector>

#include "fire_effect.h"
#include "graphics.h"
#include "logger.h"
#include "model.h"
#include "shape.h"

int main() {
	try {
		Boidsish::Visualizer vis(1280, 720, "Dissolve Demo");

		vis.GetCamera().y = 15.0;
		vis.GetCamera().z = 40.0;
		vis.SetCameraMode(Boidsish::CameraMode::STATIONARY);

		// Load a model
		auto teapot = std::make_shared<Boidsish::Model>("assets/utah_teapot.obj");
		teapot->SetPosition(0, 10, 0);
		teapot->SetScale(glm::vec3(2.0f));
		teapot->SetUsePBR(true);
		teapot->SetRoughness(0.2f);
		teapot->SetMetallic(0.8f);
		vis.AddShape(teapot);

		// Add fire effect for the dissolve
		// Use a large enough volume to cover the model
		glm::vec3 teapot_pos(teapot->GetX(), teapot->GetY(), teapot->GetZ());
		auto      dissolve_fire = vis.AddFireEffect(
            teapot_pos,
            Boidsish::FireEffectStyle::Fireflies,
            glm::vec3(0, 1, 0), // direction
            glm::vec3(0, 0, 0), // velocity
            -1,               // max particles
            -1.0f,              // lifetime
            Boidsish::EmitterType::Model,
            glm::vec3(20.0f, 20.0f, 20.0f), // dimensions
            1.0f                            // Initial sweep
        );
		vis.SetFireEffectSourceModel(dissolve_fire, teapot);

		// // Another one with sparkles
		// auto dissolve_sparks = vis.AddFireEffect(
		// 	teapot_pos,
		// 	Boidsish::FireEffectStyle::Sparks,
		// 	glm::vec3(0, 1, 0),
		// 	glm::vec3(0, 0, 0),
		// 	1000,
		// 	-1.0f,
		// 	Boidsish::EmitterType::Model,
		// 	glm::vec3(20.0f, 20.0f, 20.0f),
		// 	1.0f
		// );
		// vis.SetFireEffectSourceModel(dissolve_sparks, teapot);

		vis.AddShapeHandler([&](float time) {
			float sweep = (sin(time * 0.5f) * 0.5f + 0.5f); // Ping-pong between 0 and 1

			// Arbitrary direction that rotates
			glm::vec3 dir(sin(time * 0.3f), cos(time * 0.3f), sin(time * 0.2f));
			dir = glm::normalize(dir);

			teapot->SetDissolveSweep(dir, sweep);

			if (dissolve_fire) {
				dissolve_fire->SetSweep(sweep);
				dissolve_fire->SetDirection(dir);
			}
			// if (dissolve_sparks) {
			// 	dissolve_sparks->SetSweep(sweep);
			// 	dissolve_sparks->SetDirection(dir);
			// }

			return std::vector<std::shared_ptr<Boidsish::Shape>>{};
		});

		vis.Run();
	} catch (const std::exception& e) {
		std::cerr << "An error occurred: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
