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

	float spacing = 10.0f;
	int   cols = 3;

	for (int i = 0; i < 9; ++i) {
		float x = (i % cols - 1) * spacing;
		float y = (1 - i / cols) * spacing + 15.0f; // Offset from floor

		auto poly = std::make_shared<Polyhedron>(types[i], i + 1, x, y, 0.0f, 1.5f);
		poly->SetRotation(glm::angleAxis(time, glm::vec3(0, 1, 0.5f)));

		// PBR properties for better look
		poly->SetUsePBR(true);
		poly->SetRoughness(0.2f);
		poly->SetMetallic(0.8f);

		// Color based on index to make them distinct
		glm::vec3 colors[] = {
			{1.0f, 0.2f, 0.2f}, // Red
			{0.2f, 1.0f, 0.2f}, // Green
			{0.2f, 0.2f, 1.0f}, // Blue
			{1.0f, 1.0f, 0.2f}, // Yellow
			{1.0f, 0.2f, 1.0f}, // Magenta
			{0.2f, 1.0f, 1.0f}, // Cyan
			{1.0f, 0.5f, 0.2f}, // Orange
			{0.5f, 0.2f, 1.0f}, // Purple
			{0.8f, 0.8f, 0.8f}  // Silver
		};
		poly->SetColor(colors[i].r, colors[i].g, colors[i].b);

		shapes.push_back(poly);
	}

	return shapes;
}

int main() {
	try {
		Visualizer viz(1280, 720, "Boidsish - 9 Regular Polyhedra Demo");

		// Setup Camera - slightly further back and higher to see all 9
		Camera camera(0.0f, 8.0f, 25.0f, 0.0f, -15.0f, 45.0f);
		viz.SetCamera(camera);

		// Setup Lighting
		Light sun = Light::CreateDirectional(glm::vec3(0.5f, -1.0f, -0.5f), glm::vec3(1.0f, 0.95f, 0.8f), 2.0f);
		viz.GetLightManager().AddLight(sun);
		viz.GetLightManager().SetAmbientLight(glm::vec3(0.3f));

		// Enable Floor
		ConfigManager::GetInstance().SetBool("enable_floor", true);
		ConfigManager::GetInstance().SetBool("enable_skybox", true);

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
