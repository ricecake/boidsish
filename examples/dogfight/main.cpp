#include <iostream>
#include <memory>

#include "DogfightHandler.h"
#include "graphics.h"

using namespace Boidsish;

int main() {
	try {
		auto visualizer = std::make_shared<Visualizer>(1280, 720, "Dogfight Demo");

		auto handler = std::make_shared<DogfightHandler>(visualizer->GetThreadPool(), visualizer);
		visualizer->AddShapeHandler(std::ref(*handler));

		// Initial camera position
		auto [h, n] = visualizer->GetTerrainPropertiesAtPoint(0, 0);
		Camera camera(0.0f, h + 100.0f, 200.0f);
		visualizer->SetCamera(camera);

		std::cout << "Dogfight Demo starting..." << std::endl;
		std::cout << "Red and Blue teams will chase each other and engage in combat." << std::endl;
		std::cout << "Planes will explode if an enemy stays behind them for too long or if they hit terrain."
				  << std::endl;

		visualizer->Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
