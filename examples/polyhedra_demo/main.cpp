#include <iostream>
#include <memory>
#include <vector>

#include "ConfigManager.h"
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

	float spacing = 5.0f;
	int   cols = 3;

	for (int i = 0; i < 9; ++i) {
		float x = (i % cols - 1) * spacing;
		float y = (1 - i / cols) * spacing + 5.0f; // Offset from floor

		auto poly = std::make_shared<Polyhedron>(types[i], i + 1, x, y, 0.0f, 1.5f);
		poly->SetRotation(glm::angleAxis(time, glm::vec3(0, 1, 0.5f)));

		// PBR properties for better look
		poly->SetUsePBR(true);
		poly->SetRoughness(0.3f);
		poly->SetMetallic(0.7f);

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

		// Setup Camera
		Camera camera(0.0f, 5.0f, 20.0f, 0.0f, 0.0f, 45.0f);
		viz.SetCamera(camera);

		// Setup Lighting
		Light sun = Light::CreateDirectional(glm::vec3(0.5f, -1.0f, -0.5f), glm::vec3(1.0f, 0.95f, 0.8f), 1.5f);
		viz.GetLightManager().AddLight(sun);
		viz.GetLightManager().SetAmbientLight(glm::vec3(0.2f));

		// Enable Floor
		ConfigManager::GetInstance().SetBool("enable_floor", true);

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
