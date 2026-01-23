#include <iostream>

#include "fire_effect.h"
#include "graphics.h"
#include "logger.h"
#include "shape.h"

int main() {
	try {
		Boidsish::Visualizer vis(1280, 720, "Fire Effect Demo");

		vis.GetCamera().y = 20.0;
		vis.GetCamera().z = 80.0;
		vis.SetCameraMode(Boidsish::CameraMode::STATIONARY);

		auto mushroom_cloud = vis.AddFireEffect(
			glm::vec3(0.0f, 2.0f, 0.0f),
			Boidsish::FireEffectStyle::MushroomCloud
		);

		float                                         last_trigger_time = 0.0f;
		std::vector<std::shared_ptr<Boidsish::Shape>> vec;
		vis.AddShapeHandler([&](float time) {
			if (time - last_trigger_time > 15.0f) {
				vis.RemoveFireEffect(mushroom_cloud);
				mushroom_cloud = vis.AddFireEffect(
					glm::vec3(0.0f, 2.0f, 0.0f),
					Boidsish::FireEffectStyle::MushroomCloud
				);
				last_trigger_time = time;
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
