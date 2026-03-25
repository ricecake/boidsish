#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "graphics.h"
#include "model.h"

using namespace Boidsish;

// Morph Demo: Morphs a cube to a sphere, then to a teapot, and back.
std::vector<std::shared_ptr<Shape>> MorphExample(float time) {
	static std::shared_ptr<Model> cube = nullptr;
	static std::shared_ptr<Model> teapot = nullptr;

	if (!cube) {
		cube = std::make_shared<Model>("assets/cube.obj");
		cube->SetColor(0.2f, 0.6f, 1.0f);
		cube->SetUsePBR(true);
		cube->SetRoughness(0.3f);
		cube->SetMetallic(0.1f);
	}

	if (!teapot) {
		teapot = std::make_shared<Model>("assets/utah_teapot.obj");
		teapot->SetColor(1.0f, 0.4f, 0.2f);
		teapot->SetUsePBR(true);
		teapot->SetRoughness(0.2f);
		teapot->SetMetallic(0.8f);
	}

	static bool morph_setup = false;
	if (!morph_setup && cube && teapot) {
		// Use the new helper to match sizes and set a shared intermediate radius
		Shape::SetupMorphBetween(*cube, *teapot);
		morph_setup = true;
	}

	std::vector<std::shared_ptr<Shape>> shapes;

	// Loop time every 10 seconds
	float t = fmod(time, 10.0f);

	// Phase 0: 0-2s (Cube)
	// Phase 1: 2-4s (Morph Cube to Sphere)
	// Phase 2: 4-6s (Morph Sphere to Teapot)
	// Phase 3: 6-8s (Teapot)
	// Phase 4: 8-10s (Morph Teapot to Cube via Sphere)

	if (t < 2.0f) {
		// Just Cube
		cube->SetMorphFactor(0.0f);
		cube->SetPosition(-5.0f, 0.0f, 0.0f);
		shapes.push_back(cube);
	} else if (t < 4.0f) {
		// Cube to Sphere
		float factor = (t - 2.0f) / 2.0f;
		cube->SetMorphFactor(factor);
		cube->SetPosition(-5.0f, 0.0f, 0.0f);
		shapes.push_back(cube);
	} else if (t < 6.0f) {
		// Sphere to Teapot
		float factor = 1.0f - ((t - 4.0f) / 2.0f);
		teapot->SetMorphFactor(factor);
		teapot->SetPosition(5.0f, 0.0f, 0.0f);
		shapes.push_back(teapot);
	} else if (t < 8.0f) {
		// Just Teapot
		teapot->SetMorphFactor(0.0f);
		teapot->SetPosition(5.0f, 0.0f, 0.0f);
		shapes.push_back(teapot);
	} else {
		// Teapot back to Cube (via Sphere)
		float factor = (t - 8.0f) / 2.0f;
		if (factor < 0.5f) {
			// Teapot to Sphere
			float subFactor = factor * 2.0f;
			teapot->SetMorphFactor(subFactor);
			teapot->SetPosition(5.0f - 10.0f * factor, 0.0f, 0.0f);
			shapes.push_back(teapot);
		} else {
			// Sphere to Cube
			float subFactor = 1.0f - ((factor - 0.5f) * 2.0f);
			cube->SetMorphFactor(subFactor);
			cube->SetPosition(5.0f - 10.0f * factor, 0.0f, 0.0f);
			shapes.push_back(cube);
		}
	}

	// Add a static sphere for comparison using the same target radius
	float targetRadius = cube->GetMorphTargetRadius();
	Shape::RenderSphere(glm::vec3(0.0f, 5.0f, 0.0f), glm::vec3(0.5f), glm::vec3(targetRadius), glm::quat(1,0,0,0));

	return shapes;
}

int main() {
	try {
		Visualizer viz(1280, 720, "Boidsish - Morphing Demo");

		Camera camera(0.0f, 2.0f, 15.0f, -10.0f, 0.0f, 45.0f);
		viz.SetCamera(camera);

		viz.SetDotFunction(MorphExample);

		std::cout << "Morphing Demo Started!" << std::endl;
		std::cout << "Watch the shapes morph into spheres and back into other shapes." << std::endl;
		std::cout << "Sizes are automatically matched using Shape::SetupMorphBetween." << std::endl;

		viz.Run();

	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
