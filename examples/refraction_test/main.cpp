#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "dot.h"
#include "graphics.h"

using namespace Boidsish;

// This example demonstrates generalized screen-space refraction.
// A central sphere starts with a high index of refraction and fades down to 1.0,
// causing it to vanish as it matches the background.
std::vector<std::shared_ptr<Shape>> RefractionExample(float time) {
	std::vector<std::shared_ptr<Shape>> shapes;

	// 1. Create a large central "glass" sphere
	auto glassSphere = std::make_shared<Dot>(1, 0.0f, 10.0f, 0.0f, 300.0f); // Size 2.0 in world units

	// Index of refraction animation:
	// Cycles from 1.5 (glass-like) down to 1.0 (invisible) over a 10s period.
	float cycle = (sin(time * 0.5f) * 0.5f + 0.5f);
	float ior = 1.0f + cycle * 0.5f;

	glassSphere->SetRefractive(true, ior);
	glassSphere->SetColor(1.0f, 1.0f, 1.0f, 0.14f); // Slightly higher alpha for better surface visibility
	glassSphere->SetUsePBR(true);
	glassSphere->SetRoughness(0.05f); // Very smooth
	glassSphere->SetMetallic(0.0f);

	shapes.push_back(glassSphere);

	// 2. Create some "background" objects to see the refraction
	for (int i = 0; i < 20; ++i) {
		float angle = (float)i * (2.0f * 3.14159f / 10.0f) + time * 0.5f;
		float x = cos(angle) * (3.0f + (float)(i % 3));
		float z = sin(angle) * (3.0f + (float)(i % 3));
		float y = sin(time * 2.0f + (float)i) * 3.0f;

		auto backgroundDot = std::make_shared<Dot>(10 + i, x, y, z, 40.0f);
		backgroundDot->SetColor((float)i / 8.0f, 1.0f - (float)i / 8.0f, cycle, 1.0f);
		shapes.push_back(backgroundDot);
	}

	return shapes;
}

int main() {
	try {
		Visualizer viz(1280, 720, "Boidsish - Refraction Test");

		Camera camera(0.0f, 2.0f, 12.0f, -10.0f, 0.0f, 45.0f);
		viz.SetCamera(camera);

		viz.AddShapeHandler(RefractionExample);

		std::cout << "Refraction Test Started!" << std::endl;
		std::cout << "The central sphere's index of refraction is oscillating." << std::endl;
		std::cout << "When IOR reaches 1.0, the sphere should become perfectly invisible." << std::endl;

		viz.Run();

	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
