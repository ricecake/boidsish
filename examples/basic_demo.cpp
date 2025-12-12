#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <vector>

#include "graphics.h"
#include "platonic_solid.h"
#include "teapot.h"

using namespace Boidsish;

// Example showing the platonic solids and the Utah teapot
std::vector<std::shared_ptr<Shape>> ShapesExample(float time) {
    std::vector<std::shared_ptr<Shape>> shapes;

    // Create one of each of the first three platonic solids
    auto tetrahedron = std::make_shared<PlatonicSolid>(PlatonicSolidType::TETRAHEDRON, 0, -6.0f, 0.0f, 0.0f);
    tetrahedron->SetColor(1.0f, 0.0f, 0.0f); // Red
    shapes.push_back(tetrahedron);

    auto cube = std::make_shared<PlatonicSolid>(PlatonicSolidType::CUBE, 1, -3.0f, 0.0f, 0.0f);
    cube->SetColor(0.0f, 1.0f, 0.0f); // Green
    shapes.push_back(cube);

    auto octahedron = std::make_shared<PlatonicSolid>(PlatonicSolidType::OCTAHEDRON, 2, 0.0f, 0.0f, 0.0f);
    octahedron->SetColor(0.0f, 0.0f, 1.0f); // Blue
    shapes.push_back(octahedron);

    auto dodecahedron = std::make_shared<PlatonicSolid>(PlatonicSolidType::DODECAHEDRON, 3, 3.0f, 0.0f, 0.0f);
    dodecahedron->SetColor(1.0f, 1.0f, 0.0f); // Yellow
    shapes.push_back(dodecahedron);

    auto icosahedron = std::make_shared<PlatonicSolid>(PlatonicSolidType::ICOSAHEDRON, 4, 6.0f, 0.0f, 0.0f);
    icosahedron->SetColor(1.0f, 0.0f, 1.0f); // Magenta
    shapes.push_back(icosahedron);

    auto teapot = std::make_shared<Teapot>(5, 9.0f, 0.0f, 0.0f);
    teapot->SetColor(0.0f, 1.0f, 1.0f); // Cyan
    shapes.push_back(teapot);

    return shapes;
}

int main() {
	try {
		// Create the visualizer
		Visualizer viz(1024, 768, "Boidsish - Shapes Example");

		// Set up the initial camera position
		Camera camera(0.0f, 2.0f, 15.0f, -15.0f, 0.0f, 45.0f);
		viz.SetCamera(camera);

		// Set the shape function
		viz.AddShapeHandler(ShapesExample);

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
