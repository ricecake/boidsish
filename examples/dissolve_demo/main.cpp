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
            -1,                 // max particles
            -1.0f,              // lifetime
            Boidsish::EmitterType::Model,
            glm::vec3(20.0f, 20.0f, 20.0f), // dimensions
            1.0f                            // Initial sweep
        );
		vis.SetFireEffectSourceModel(dissolve_fire, teapot);

		struct DemoState {
			float sweep = 1.0f;
			float sweep_direction = -1.0f;
			float wait_timer = 0.0f;
			bool  is_waiting = false;
		} state;

		vis.AddShapeHandler([&](float /* time */) {
			float delta_time = 1.0f / 60.0f; // Approximate

			if (state.is_waiting) {
				state.wait_timer -= delta_time;
				if (state.wait_timer <= 0.0f) {
					state.is_waiting = false;
				}
				return std::vector<std::shared_ptr<Boidsish::Shape>>{};
			}

			state.sweep += state.sweep_direction * delta_time * 0.5f;

			bool finished = false;
			if (state.sweep <= 0.0f) {
				state.sweep = 0.0f;
				state.sweep_direction = 1.0f;
				finished = true;
			} else if (state.sweep >= 1.0f) {
				state.sweep = 1.0f;
				state.sweep_direction = -1.0f;
				finished = true;
			}

			// Arbitrary direction that stays constant for one sweep but could be rotated
			glm::vec3 dir(0, 1, 0);
			teapot->SetDissolveSweep(dir, state.sweep);

			if (dissolve_fire) {
				dissolve_fire->SetSweep(state.sweep);
				dissolve_fire->SetDirection(dir);

				if (finished) {
					dissolve_fire->ClearParticles();
					state.is_waiting = true;
					state.wait_timer = 1.0f;
				}
			}

			return std::vector<std::shared_ptr<Boidsish::Shape>>{};
		});

		vis.Run();
	} catch (const std::exception& e) {
		std::cerr << "An error occurred: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
