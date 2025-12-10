#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "graph.h"
#include "graphics.h"

using namespace Boidsish;

// Example: Graph with spline smoothing
std::vector<std::shared_ptr<Shape>> GraphExample(float time) {
	std::vector<std::shared_ptr<Shape>> shapes;
	auto                                graph = std::make_shared<Graph>(0, 0, 0, 0);

	// Add vertices in a chain
	graph->vertices.push_back({Vector3(-4, 0, 0), 10.0f, 1, 0, 0, 1});
	graph->vertices.push_back({Vector3(-2, 2, 0), 12.0f, 0, 1, 0, 1});
	graph->vertices.push_back({Vector3(0, 0, 0), 15.0f, 0, 0, 1, 1});
	graph->vertices.push_back({Vector3(2, -2, 0), 12.0f, 1, 1, 0, 1});
	graph->vertices.push_back({Vector3(4, 0, 0), 10.0f, 1, 0, 1, 1});

	// Add edges to connect the vertices in a chain
	graph->edges.push_back({0, 1});
	graph->edges.push_back({1, 2});
	graph->edges.push_back({2, 3});
	graph->edges.push_back({3, 4});

	shapes.push_back(graph);
	return shapes;
}

int main(int argc, char* argv[]) {
    bool close_on_finish = false;
    std::string screenshot_path = "";
    int width = 1024;
    int height = 768;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--close-on-finish" && i + 1 < argc) {
            close_on_finish = std::stoi(argv[++i]);
        } else if (arg == "--screenshot" && i + 1 < argc) {
            screenshot_path = argv[++i];
        } else if (arg == "--width" && i + 1 < argc) {
            width = std::stoi(argv[++i]);
        } else if (arg == "--height" && i + 1 < argc) {
            height = std::stoi(argv[++i]);
        }
    }

	try {
		// Create the visualizer
		Visualizer viz(width, height, "Boidsish - Simple 3D Visualization Example");

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

        if (close_on_finish) {
            viz.Update();
            viz.Render();
            if (!screenshot_path.empty()) {
                viz.SaveScreenshot(screenshot_path);
            }
        } else {
            viz.Run();
        }

		std::cout << "Visualization ended." << std::endl;

	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
