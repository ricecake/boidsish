#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>

#include "boidsish.h"
#include "graph.h"

using namespace Boidsish;

// Example: Simple circular motion with multiple dots
std::vector<std::shared_ptr<Shape>> GraphExample(float time) {
    std::vector<std::shared_ptr<Shape>> shapes;
    auto graph = std::make_shared<Graph>();

    // Add vertices
    graph->vertices.push_back({Vector3(0, 0, 0), 10.0f, 1, 0, 0, 1});
    graph->vertices.push_back({Vector3(2, 2, 0), 12.0f, 0, 1, 0, 1});
    graph->vertices.push_back({Vector3(-2, 2, 0), 8.0f, 0, 0, 1, 1});
    graph->vertices.push_back({Vector3(0, -2, 0), 15.0f, 1, 1, 0, 1});

    // Add edges
    graph->edges.push_back({0, 1});
    graph->edges.push_back({0, 2});
    graph->edges.push_back({0, 3});
    graph->edges.push_back({1, 2});

    shapes.push_back(graph);
    return shapes;
}

int main() {
	try {
		// Create the visualizer
		Visualizer viz(1024, 768, "Boidsish - Simple 3D Visualization Example");

		// Set up the initial camera position
		Camera camera(0.0f, 2.0f, 8.0f, -15.0f, 0.0f, 45.0f);
		viz.SetCamera(camera);

		// Set the dot function
		viz.SetDotFunction(GraphExample);

		auto path = std::filesystem::current_path();

		std::cout << "CWD: " << path.string() << std::endl;
		std::cout << "Boidsish 3D Visualizer Started!" << std::endl;
		std::cout << "Controls:" << std::endl;
		std::cout << "  WASD - Move camera horizontally" << std::endl;
		std::cout << "  Space/Shift - Move camera up/down" << std::endl;
		std::cout << "  Mouse - Look around" << std::endl;
		std::cout << "  ESC - Exit" << std::endl;
		std::cout << std::endl;

		// Run the visualization
		viz.Run();

		std::cout << "Visualization ended." << std::endl;

	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}