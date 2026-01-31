#include <functional>
#include <iostream>

#include "SdfBoidHandler.h"
#include "graphics.h"

using namespace Boidsish;

int main() {
	try {
		// Create the visualizer
		auto viz = std::make_shared<Visualizer>(1200, 800, "Boidsish - SDF Boids Example");

		// Set up camera
		Camera camera(0.0f, 0.0f, 250.0f, 0.0f, 0.0f, 0.0f);
		viz->SetCamera(camera);

		// Create and set the entity handler
		SdfBoidHandler handler(viz->GetThreadPool(), viz);
		viz->AddShapeHandler(std::ref(handler));

		// Run the visualization
		viz->Run();

	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
