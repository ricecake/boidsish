#include "boidsish.h"
#include <cmath>
#include <iostream>

using namespace Boidsish;

// Example: Simple circular motion with multiple dots
std::vector<Dot> CircularMotionExample(float time) {
    std::vector<Dot> dots;

    int num_dots = 5 + int(time/15);
    for (int i = 0; i < num_dots; ++i) {
        float angle = time * 0.5f + i * 2.0f * M_PI / num_dots;
        float radius = 3.0f + i * 0.5f;

        float x = cos(angle) * radius;
        float y = sin(time * 0.3f + i) * 2.0f;
        float z = sin(angle) * radius;

        // Different colors for each dot
        float r = 0.5f + 0.5f * sin(time * 0.1f + i * 0.7f);
        float g = 0.5f + 0.5f * cos(time * 0.15f + i * 1.1f);
        float b = 0.5f + 0.5f * sin(time * 0.2f + i * 1.3f);

        float size = 8.0f + 4.0f * sin(time * 0.4f + i);
        int trail_length = 50 + i * 20; // Much longer trails

        dots.emplace_back(i, x, y, z, size, r, g, b, 1.0f, trail_length);
    }

    return dots;
}

int main() {
    try {
        // Create the visualizer
        Visualizer viz(1024, 768, "Boidsish - Simple 3D Visualization Example");

        // Set up the initial camera position
        Camera camera(0.0f, 2.0f, 8.0f, -15.0f, 0.0f, 45.0f);
        viz.SetCamera(camera);

        // Set the dot function
        viz.SetDotFunction(CircularMotionExample);

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