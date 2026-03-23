#include <iostream>
#include <memory>
#include <vector>

#include "graphics.h"
#include "polyhedron.h"

using namespace Boidsish;

std::vector<std::shared_ptr<Shape>> PolyhedraDemo(float time) {
	std::vector<std::shared_ptr<Shape>> shapes;

	PolyhedronType types[] = {
		PolyhedronType::Tetrahedron,
		PolyhedronType::Cube,
		PolyhedronType::Octahedron,
		PolyhedronType::Dodecahedron,
		PolyhedronType::Icosahedron,
		PolyhedronType::SmallStellatedDodecahedron,
		PolyhedronType::GreatDodecahedron,
		PolyhedronType::GreatStellatedDodecahedron,
		PolyhedronType::GreatIcosahedron
	};

	float spacing = 4.0f;
	int   cols = 3;

	for (int i = 0; i < 9; ++i) {
		float x = (i % cols - 1) * spacing;
		float y = (i / cols - 1) * spacing;

		auto poly = std::make_shared<Polyhedron>(types[i], i + 1, x, y, 0.0f, 1.0f);
		poly->SetRotation(glm::angleAxis(time, glm::vec3(0, 1, 0.5f)));

		// Color based on index
		float r = (i % 3) / 2.0f;
		float g = ((i / 3) % 3) / 2.0f;
		float b = 1.0f - r;
		poly->SetColor(r, g, b);

		shapes.push_back(poly);
	}

	return shapes;
}

int main() {
	try {
		Visualizer viz(1280, 720, "Boidsish - 9 Regular Polyhedra Demo");

		Camera camera(0.0f, 0.0f, 15.0f, 0.0f, 0.0f, 45.0f);
		viz.SetCamera(camera);

		viz.SetDotFunction(PolyhedraDemo);

		std::cout << "Polyhedra Demo Started!" << std::endl;
		std::cout << "Displaying all 9 regular polyhedra (5 Platonic + 4 Kepler-Poinsot)" << std::endl;

		viz.Run();

	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
