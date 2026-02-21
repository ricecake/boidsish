#include <iostream>
#include <vector>

#include "graphics.h"
#include "logger.h"
#include "model.h"
#include "decor_manager.h"

int main() {
	try {
		Boidsish::Visualizer visualizer(1280, 720, "Biome & Detail Demo");

		// Set a custom camera position
		Boidsish::Camera camera;
		camera.x = 100.0f;
		camera.y = 50.0f;
		camera.z = 100.0f;
		camera.pitch = -20.0f;
		camera.yaw = -135.0f;
		visualizer.SetCamera(camera);

		auto decor = visualizer.GetDecorManager();
		if (decor) {
			// Lush Trees (growing in Lush Grass and Forest biomes - indices 1 and 3)
			Boidsish::DecorProperties treeProps;
			treeProps.max_density = 0.8f;
			treeProps.base_scale = 1.5f;
			treeProps.scale_variance = 0.5f;
			treeProps.biome_mask = (1u << 1) | (1u << 3);
			decor->AddDecorType("assets/tree01.obj", treeProps);

			// Dead Trees (growing in Dry Grass and Alpine Meadow - indices 2 and 4)
			Boidsish::DecorProperties deadTreeProps;
			deadTreeProps.max_density = 0.4f;
			deadTreeProps.base_scale = 1.2f;
			deadTreeProps.biome_mask = (1u << 2) | (1u << 4);
			decor->AddDecorType("assets/PUSHILIN_dead_tree.obj", deadTreeProps);

			// Rocky Details (small "rocks" only visible at close range in Rocky biomes - indices 5, 6)
			Boidsish::DecorProperties rockProps;
			rockProps.max_density = 1.5f; // High density for details
			rockProps.base_scale = 0.2f;
			rockProps.scale_variance = 0.1f;
			rockProps.biome_mask = (1u << 5) | (1u << 6);
			rockProps.detail_distance = 100.0f; // Only visible if camera is within 100 units
			rockProps.align_to_terrain = true;
			decor->AddDecorType("assets/cube.obj", rockProps);
		}

		visualizer.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
