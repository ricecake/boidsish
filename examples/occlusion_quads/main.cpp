#include <iostream>
#include <vector>

#include "graphics.h"
#include "logger.h"
#include "model.h"

int main() {
	try {
		Boidsish::Visualizer visualizer(1280, 720, "Occlusion Quads Demo");

		// Set a custom camera position
		Boidsish::Camera camera;
		camera.x = 16.0f;
		camera.y = 50.0f;
		camera.z = 16.0f;
		camera.pitch = -45.0f;
		camera.yaw = -45.0f;
		visualizer.SetCamera(camera);

		// Enable occluder visualization by default
		visualizer.SetOccluderVisualizationEnabled(true);

		visualizer.AddInputCallback([&visualizer](const Boidsish::InputState& state) {
			if (state.key_down[GLFW_KEY_O]) {
				bool enabled = visualizer.IsOccluderVisualizationEnabled();
				visualizer.SetOccluderVisualizationEnabled(!enabled);
				logger::LOG("Occluder visualization: " + std::string(!enabled ? "ENABLED" : "DISABLED"));
			}
		});

		std::vector<std::shared_ptr<Boidsish::Shape>> shapes;
		visualizer.AddShapeHandler([&](float time) { return shapes; });

		auto model = std::make_shared<Boidsish::Model>("assets/utah_teapot.obj");
		model->SetColossal(true);
		model->SetPosition(0, 100, 0);
		shapes.push_back(model);

		std::cout << "Controls:" << std::endl;
		std::cout << "  O: Toggle occluder quad visualization" << std::endl;
		std::cout << "  WASD/Space/Shift: Move camera" << std::endl;
		std::cout << "  Mouse: Look around" << std::endl;

		visualizer.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
