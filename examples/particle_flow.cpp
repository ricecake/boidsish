#include <iostream>
#include <vector>

#include "graphics.h"

int main() {
	try {
		Boidsish::Visualizer visualizer(1280, 720, "Particle Flow Demo");

		// Set a custom camera position
		Boidsish::Camera camera;
		camera.x = 16.0f;
		camera.y = 10.0f;
		camera.z = 16.0f;
		camera.pitch = -30.0f;
		camera.yaw = -45.0f;
		visualizer.SetCamera(camera);

		// No shapes, just terrain and particles handled by the visualizer
		visualizer.AddShapeHandler([](float /* time */) { return std::vector<std::shared_ptr<Boidsish::Shape>>(); });

		visualizer.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
