#include <iostream>
#include <memory>

#include "graphics.h"
#include "path.h"

using namespace Boidsish;

int main() {
	try {
		auto visualizer = std::make_shared<Visualizer>(1280, 720, "Path Camera Demo");

		auto path_handler = std::make_shared<PathHandler>();
		auto path = path_handler->AddPath();
		path->SetVisible(true);

		// Define a path with some turns and elevation changes
		path->AddWaypoint({-20, 5, -20}, {0, 1, 0});
		path->AddWaypoint({20, 5, -20}, {0, 1, 0});
		path->AddWaypoint({20, 15, 20}, {0, 1, 1});   // Bank inwards
		path->AddWaypoint({-20, 15, 20}, {0, 1, -1}); // Bank inwards
		path->SetMode(PathMode::LOOP);

		visualizer->AddShapeHandler([&](float time) { return path_handler->GetShapes(); });

		visualizer->SetPathCamera(path);

		visualizer->Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
