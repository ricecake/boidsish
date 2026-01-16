#include <functional>
#include <iostream>

#include "VortexFlockingHandler.h"
#include "graphics.h"

using namespace Boidsish;

int main() {
	try {
		// Create the visualizer as a shared_ptr to manage its lifetime
		auto viz = std::make_shared<Visualizer>(1200, 800, "Boidsish - Vortex Flocking Example");

		// Set up camera
		Camera camera(0.0f, 50.0f, 150.0f, -30.0f, -90.0f, 0.0f);
		viz->SetCamera(camera);

		// Create and set the entity handler
		VortexFlockingHandler handler(viz->GetThreadPool(), viz);
		viz->AddShapeHandler(std::ref(handler));

		// Run the visualization
		viz->Run();

	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
