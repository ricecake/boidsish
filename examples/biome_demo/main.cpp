#include <iostream>
#include <vector>

#include "decor_manager.h"
#include "graphics.h"
#include "logger.h"
#include "model.h"

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
			treeProps.min_height = 0.01;
			treeProps.max_height = 95.0f;
			treeProps.min_density = 0.1f;
			treeProps.max_density = 0.11f;
			treeProps.base_scale = 0.008f;
			treeProps.scale_variance = 0.01f;
			treeProps.biomes = {Boidsish::Biome::LushGrass, Boidsish::Biome::Forest};
			decor->AddDecorType("assets/tree01.obj", treeProps);

			// Dead Trees (growing in Dry Grass and Alpine Meadow)
			Boidsish::DecorProperties deadTreeProps;
			deadTreeProps.min_height = 30.0f;
			deadTreeProps.max_height = 95.0f;
			deadTreeProps.min_density = 0.1f;
			deadTreeProps.max_density = 0.11f;
			deadTreeProps.base_scale = 0.8f;
			deadTreeProps.scale_variance = 0.01f;
			deadTreeProps.biomes = {Boidsish::Biome::DryGrass, Boidsish::Biome::AlpineMeadow};
			decor->AddDecorType("assets/PUSHILIN_dead_tree.obj", deadTreeProps);

			// Rocky Details (small "rocks" only visible at close range in Rocky biomes)
			Boidsish::DecorProperties rockProps;
			rockProps.max_density = 1.5f; // High density for details
			rockProps.base_scale = 0.002f;
			rockProps.scale_variance = 0.1f;
			rockProps.biomes = {Boidsish::Biome::BrownRock, Boidsish::Biome::GreyRock};
			// rockProps.detail_distance = 100.0f; // Only visible if camera is within 100 units
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
