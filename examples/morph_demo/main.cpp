#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "graphics.h"
#include "model.h"

using namespace Boidsish;

// Morph Demo: Morphs between a cube and a teapot using a sphere as an intermediate.
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
		// Helper automates size matching and intermediate radius calculation
		Shape::SetupMorphBetween(*cube, *teapot);
		morph_setup = true;
	}

	std::vector<std::shared_ptr<Shape>> shapes;

	float factor = 0.0f;
	bool show_a = true;
	float cycle_duration = 5.0f; // 5 seconds for A -> Sphere -> B

	// Logic for A -> Sphere -> B transition
	Shape::ComputeMorphState(time, cycle_duration, factor, show_a);

	if (show_a) {
		cube->SetMorphFactor(factor);
		cube->SetPosition(0.0f, 0.0f, 0.0f);
		shapes.push_back(cube);
	} else {
		teapot->SetMorphFactor(factor);
		teapot->SetPosition(0.0f, 0.0f, 0.0f);
		shapes.push_back(teapot);
	}

	// Add a static sphere for comparison using the same target radius
	float targetRadius = cube->GetMorphTargetRadius();
	Shape::RenderSphere(glm::vec3(0.0f, 8.0f, 0.0f), glm::vec3(0.5f), glm::vec3(targetRadius), glm::quat(1,0,0,0));

	return shapes;
}

int main() {
	try {
		Visualizer viz(1280, 720, "Boidsish - Smoother Morphing Demo");

		Camera camera(0.0f, 5.0f, 15.0f, -15.0f, 0.0f, 45.0f);
		viz.SetCamera(camera);

		viz.SetDotFunction(MorphExample);

		std::cout << "Smoother Morphing Demo Started!" << std::endl;
		std::cout << "Using Shape::ComputeMorphState for timing logic and SetupMorphBetween for sizing." << std::endl;

		viz.Run();

	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
