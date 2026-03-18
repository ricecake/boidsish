#include "graphics.h"
#include "logger.h"
#include <glm/glm.hpp>
#include <memory>
#include <vector>

using namespace Boidsish;

int main() {
	Visualizer viz(1280, 720, "SDF Explosion Demo");

	viz.GetCamera().x = 0;
	viz.GetCamera().y = 5;
	viz.GetCamera().z = 30;
	viz.GetCamera().pitch = -10;

	float timer = 0.0f;
	float spawn_interval = 3.5f;

	// viz.AddInputCallback([&](const InputState& state) {
	// 	if (state.mouse_button_down[0]) {
	// 		viz.AddSdfExplosion(glm::vec3(-15, 5, 0), 10.0f, 2.5f);
	// 		logger::INFO("Spawned SDF Explosion via Mouse Click");
	// 	}
	// 	if (state.mouse_button_down[1]) {
	// 		viz.AddSdfVolumetricExplosion(glm::vec3(15, 5, 0), 10.0f, 2.5f);
	// 		logger::INFO("Spawned SDF Volumetric Explosion via Right Click");
	// 	}
	// });

	// viz.AddPrepareCallback([&](Visualizer& v) {
	// 	// v.AddSdfExplosion(glm::vec3(-15, 5, 0), 10.0f, 2.5f);
	// 	v.AddSdfVolumetricExplosion(glm::vec3(15, 5, 0), 10.0f, 15.5f);
	// });

	viz.AddSdfVolumetricExplosion(glm::vec3(15, 5, 0), 10.0f, 3.0f);
	while (!viz.ShouldClose()) {
		viz.Update();

		timer += viz.GetLastFrameTime();
		if (timer >= spawn_interval) {
			// viz.AddSdfExplosion(glm::vec3(-15, 5, 0), 10.0f, 2.5f);
			viz.AddSdfVolumetricExplosion(glm::vec3(15, 5, 0), 20.0f, 3.0f);
			timer = 0.0f;
		}

		viz.Render();
	}

	return 0;
}
