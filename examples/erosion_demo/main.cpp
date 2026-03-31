#include <iostream>
#include <vector>

#include "graphics.h"
#include "logger.h"
#include "model.h"

int main() {
	try {
		Boidsish::Visualizer visualizer(1280, 720, "Terrain Erosion Demo");

		// Set a custom camera position to see the terrain detail
		Boidsish::Camera camera;
		camera.x = 200.0f;
		camera.y = 80.0f;
		camera.z = 200.0f;
		camera.pitch = -35.0f;
		camera.yaw = -135.0f;
		visualizer.SetCamera(camera);

		std::cout << "====================================================" << std::endl;
		std::cout << "Terrain Erosion Demo" << std::endl;
		std::cout << "Applying Rune Skovbo Johansen's Erosion Filter" << std::endl;
		std::cout << "The filter is integrated into terrain.frag" << std::endl;
		std::cout << "====================================================" << std::endl;

		visualizer.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
