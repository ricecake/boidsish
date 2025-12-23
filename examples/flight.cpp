#include <iostream>
#include <vector>

#include "arrow.h"
#include "dot.h"
#include "graphics.h"
using namespace Boidsish;

int main() {
	try {
		Boidsish::Visualizer visualizer(1280, 720, "Terrain Demo");

		// Set a custom camera position
		Boidsish::Camera camera;
		camera.x = 16.0f;
		camera.y = 10.0f;
		camera.z = 16.0f;
		camera.pitch = 0.0f;
		camera.yaw = 0.0f;
		visualizer.SetCamera(camera);

		// No shapes, just terrain
		visualizer.AddShapeHandler([&](float) {
			auto cam = visualizer.GetCamera();
			auto shapes = std::vector<std::shared_ptr<Boidsish::Shape>>();
			auto dot = std::make_shared<Dot>(1);
			auto front = cam.front();
			auto dotPost = cam.pos() + 3.0f * cam.front();

			// dot->SetSize(15);
			dot->SetPosition(dotPost.x, dotPost.y, dotPost.z);
			dot->SetTrailLength(0); // Enable trails
			dot->SetColor(1.0f, 0.5f, 0.0f);
			shapes.push_back(dot);

			auto arrow2 = std::make_shared<Boidsish::Arrow>(1, 0, 0, 0, 0.1f, 0.1f, 0.01f, 0.0f, 1.0f, 0.0f);
			arrow2->SetPosition(dotPost.x, dotPost.y, dotPost.z);
			arrow2->SetScale(glm::vec3(1.0f, 1.0f, 1.0f));

			shapes.push_back(arrow2);

			return shapes;
		});

		visualizer.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
