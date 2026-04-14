#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

#include "decor_manager.h"
#include "frustum.h"
#include "graphics.h"
#include "service_locator.h"
#include "terrain_generator.h"
#include "terrain_render_manager.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>

using namespace Boidsish;

int main() {
	if (!glfwInit())
		return -1;
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	GLFWwindow* window = glfwCreateWindow(640, 480, "Dummy", NULL, NULL);
	if (!window)
		return -1;
	glfwMakeContextCurrent(window);
	glewInit();

	ServiceLocator locator;
	auto           decor_mgr = std::make_shared<DecorManager>(locator);
	auto           terrain_mgr = std::make_shared<TerrainRenderManager>(locator, 32, 512);
	auto           terrain_gen = std::make_unique<TerrainGenerator>();
	terrain_gen->SetRenderManager(terrain_mgr);

	// Increase density to ensure something is placed
	DecorProperties high_density_props;
	high_density_props.min_density = 1.0f;
	high_density_props.max_density = 1.0f;
	high_density_props.biomes = BiomeBitset(
		{Biome::Sand,
	     Biome::LushGrass,
	     Biome::DryGrass,
	     Biome::Forest,
	     Biome::AlpineMeadow,
	     Biome::BrownRock,
	     Biome::GreyRock,
	     Biome::Snow}
	);

	decor_mgr->AddDecorType("assets/decor/Tree/tree01.obj", high_density_props);
	decor_mgr->PrepareResources(nullptr); // Ensure UBOs are created

	// Simulate some terrain generation and registration
	Camera camera;
	camera.x = 16;
	camera.y = 50;
	camera.z = 16;

	// Create a frustum that definitely sees the area
	glm::mat4 view = glm::lookAt(glm::vec3(16, 100, 16), glm::vec3(16, 0, 16), glm::vec3(0, 0, -1));
	glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 1000.0f);
	Frustum   frustum = Frustum::FromViewProjection(view, proj);

	std::cout << "Waiting for terrain chunks to generate..." << std::endl;

	int chunks_found = 0;
	for (int i = 0; i < 100; ++i) {
		terrain_gen->Update(frustum, camera);
		decor_mgr->Update(0.016f, camera, frustum, *terrain_gen, terrain_mgr);

		chunks_found = terrain_mgr->GetRegisteredChunkCount();
		if (chunks_found >= 1) {
			auto chunk_data = terrain_mgr->GetDecorChunkData(terrain_gen->GetWorldScale());
			bool found_target = false;
			for (const auto& cd : chunk_data) {
				if (cd.key.first == 0 && cd.key.second == 0) {
					found_target = true;
					break;
				}
			}
			if (found_target)
				break;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	std::cout << "Registered chunks: " << chunks_found << std::endl;

	std::vector<std::pair<int, int>> keys = {{0, 0}};
	auto                             results = decor_mgr->GetDecorInChunks(keys, terrain_mgr, *terrain_gen);

	std::cout << "Query results for " << keys.size() << " chunks:" << std::endl;
	bool found_any = false;
	for (const auto& res : results) {
		if (res.instances.empty())
			continue;
		found_any = true;
		std::cout << "Type: " << res.model_path << " - " << res.instances.size() << " instances" << std::endl;
		for (size_t i = 0; i < std::min(res.instances.size(), size_t(2)); ++i) {
			const auto& inst = res.instances[i];
			std::cout << "  Instance " << i << ": pos(" << inst.center.x << ", " << inst.center.y << ", "
			          << inst.center.z << ")" << std::endl;
		}
	}

	if (!found_any) {
		std::cout << "No decor instances found in the requested chunks." << std::endl;
	}

	glfwTerminate();
	return 0;
}
