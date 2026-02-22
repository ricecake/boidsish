#include <iostream>
#include <vector>

#include "ConfigManager.h"
#include "decor_manager.h"
#include "graphics.h"
#include "logger.h"
#include "model.h"

int main() {
	try {
		Boidsish::Visualizer visualizer(1280, 720, "Wind Test Demo");

		// Set a custom camera position
		Boidsish::Camera camera;
		camera.x = 0.0f;
		camera.y = 20.0f;
		camera.z = 20.0f;
		camera.pitch = -30.0f;
		camera.yaw = 0.0f;
		visualizer.SetCamera(camera);

		auto decor = visualizer.GetDecorManager();
		if (decor) {
			// Tall "grass" (cubes)
			Boidsish::DecorProperties grassProps;
			grassProps.max_density = 1.0f;
			grassProps.base_scale = 0.5f;
			grassProps.scale_variance = 0.2f;
			grassProps.align_to_terrain = true;
			decor->AddDecorType("assets/cube.obj", grassProps);

			// Trees
			Boidsish::DecorProperties treeProps;
			treeProps.max_density = 0.2f;
			treeProps.base_scale = 0.01f;
			treeProps.scale_variance = 0.005f;
			decor->AddDecorType("assets/tree01.obj", treeProps);
		}

		// Set wind defaults in config for the test
		auto& config = Boidsish::ConfigManager::GetInstance();
		config.SetFloat("wind_strength", 1.5f);
		config.SetFloat("wind_speed", 2.0f);
		config.SetFloat("wind_frequency", 0.05f);

		visualizer.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
