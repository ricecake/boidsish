#include <functional>
#include <iostream>

#include "GraphExample.h"
#include "VectorDemoHandler.h"
#include "fire_effect_manager.h"
#include "graphics.h"
#include <GLFW/glfw3.h>

using namespace Boidsish;

int main() {
	try {
		Boidsish::Visualizer viz(1200, 800, "Vector3 Operations Demo");

		// Set up camera
		Camera camera;
		camera.x = 0.0f;
		camera.y = 50.0f;
		camera.z = 50.0f;
		camera.yaw = 0.0f;
		camera.pitch = -45.0f;
		viz.SetCamera(camera);

		// Create and set the vector demo handler
		VectorDemoHandler handler(viz.GetThreadPool(), viz);
		viz.AddShapeHandler(std::ref(handler));
		viz.AddShapeHandler(std::ref(GraphExample));

		std::cout << "Vector Demo Started!" << std::endl;
		std::cout << "Controls:" << std::endl;
		std::cout << "  WASD - Move camera" << std::endl;
		std::cout << "  Space/Shift - Move up/down" << std::endl;
		std::cout << "  Mouse - Look around" << std::endl;
		std::cout << "  0 - Toggle auto-camera" << std::endl;
		std::cout << "  ESC - Exit" << std::endl;

		viz.Run();
		std::cout << "Vector demo ended." << std::endl;

	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
