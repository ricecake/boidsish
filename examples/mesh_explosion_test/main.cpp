#include <iostream>
#include <memory>
#include <vector>

#include "dot.h"
#include "graphics.h"
#include "model.h"
#include "shape.h"
#include <glm/gtc/random.hpp>

using namespace Boidsish;

int main() {
	std::cout << "Starting Mesh Explosion Test..." << std::endl;
	Visualizer visualizer(1024, 768, "Mesh Explosion Test");

	// Create a teapot model
	auto teapot = std::make_shared<Model>("assets/utah_teapot.obj");
	teapot->SetColor(1.0f, 0.0f, 0.0f, 1.0f); // Red teapot
	teapot->SetScale(glm::vec3(5.0f));

	// Create a dot (procedural sphere)
	auto dot = std::make_shared<Dot>();
	dot->SetColor(0.0f, 0.0f, 1.0f, 1.0f); // Blue dot
	dot->SetSize(10.0f);

	std::vector<std::shared_ptr<Shape>> shapes;

	auto handler = [&](float t) {
		static float last_explode = -10.0f;
		if (t - last_explode > 3.0f) {
			last_explode = t;
			static int count = 0;
			if (count % 2 == 0) {
				std::cout << "Exploding Red Teapot!" << std::endl;
				visualizer.ExplodeShape(teapot, 5.0f);
			} else {
				std::cout << "Exploding Blue Dot!" << std::endl;
				visualizer.ExplodeShape(dot, 1.0f);
			}
			count++;
		}
		return shapes;
	};

	visualizer.AddShapeHandler(handler);

	// Set a good camera position
	Camera cam;
	cam.x = 0.0f;
	cam.y = 40.0f;
	cam.z = 100.0f;
	cam.pitch = -20.0f;
	cam.yaw = 0.0f;
	visualizer.SetCamera(cam);

	std::cout << "Running visualizer loop..." << std::endl;
	visualizer.Run();

	return 0;
}
