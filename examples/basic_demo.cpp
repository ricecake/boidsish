#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>

#include "dot.h"
#include "graph.h"
#include "graphics.h"

using namespace Boidsish;

// Example: Simple moving dot with trail
std::vector<std::shared_ptr<Shape>> TrailExample(float time) {
	std::vector<std::shared_ptr<Shape>> shapes;

	// Example: Graph with spline smoothing
	auto graph = std::make_shared<Graph>(0, 0, 0, 0);

	// Add vertices in a chain
	graph->AddVertex(Vector3(-4, 0, 0), 10.0f, 1, 0, 0, 1);
	graph->AddVertex(Vector3(-2, 2, 0), 12.0f, 0, 1, 0, 1);
	graph->AddVertex(Vector3(0, 0, 0), 15.0f, 0, 0, 1, 1);
	graph->AddVertex(Vector3(2, -2, 0), 12.0f, 1, 1, 0, 1);
	graph->AddVertex(Vector3(4, 0, 0), 10.0f, 1, 0, 1, 1);

	// Add edges to connect the vertices in a chain
	graph->V(0).Link(graph->V(1));
	graph->V(1).Link(graph->V(2));
	graph->V(2).Link(graph->V(3));
	graph->V(3).Link(graph->V(4));

	shapes.push_back(graph);

	// Example: Simple moving dot with trail
	// Create a moving dot with a trail
	auto dot = std::make_shared<Dot>(1, sin(time) * 3.0f, cos(time * 0.7f) * 2.0f, sin(time * 0.5f) * 1.5f);
	dot->SetTrailLength(100); // Enable trails
	dot->SetColor(1.0f, 0.5f, 0.0f);
	shapes.push_back(dot);

	// Another moving dot
	auto dot2 = std::make_shared<Dot>(2, cos(time * 1.3f) * 2.5f, sin(time * 0.9f) * 2.0f, cos(time * 0.8f) * 1.0f);
	dot2->SetTrailLength(150);
	dot2->SetColor(0.0f, 1.0f, 0.5f);
	shapes.push_back(dot2);

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
		viz.SetDotFunction(TrailExample);

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