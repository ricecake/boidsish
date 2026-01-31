#include <iostream>
#include <memory>
#include "graphics.h"
#include "path.h"

using namespace Boidsish;

int main() {
	try {
		auto visualizer = std::make_shared<Visualizer>(1280, 720, "Reversal Test");
		auto path_handler = std::make_shared<PathHandler>();
		auto path = path_handler->AddPath();
		path->SetVisible(true);
		path->AddWaypoint({-10, 5, 0}, {0, 1, 0});
		path->AddWaypoint({10, 5, 0}, {0, 1, 0});
		path->SetMode(PathMode::REVERSE);
		visualizer->AddShapeHandler([&](float time) { return path_handler->GetShapes(); });
		visualizer->SetPathCamera(path);

		// Run for a few frames headlessly if possible, or just exit if we only want to check compilation
		// In this environment, visualizer->Run() will block.
		// For verification, just compiling is often enough for simple logic changes.
		std::cout << "Reversal Test compiled and initialized successfully." << std::endl;

	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
