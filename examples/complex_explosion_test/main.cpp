#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>

#include "dot.h"
#include "graphics.h"
#include "model.h"
#include "shape.h"
#include <GLFW/glfw3.h>

using namespace Boidsish;

int main() {
	std::cout << "Starting Complex Explosion Test (Canonical)..." << std::endl;
	Visualizer visualizer(1024, 768, "Complex Explosion Test");

	std::vector<std::shared_ptr<Shape>> shapes;

	// Create a teapot model
	auto teapot = std::make_shared<Model>("assets/utah_teapot.obj");
	teapot->SetColor(1.0f, 0.5f, 0.0f, 1.0f); // Orange teapot
	teapot->SetScale(glm::vec3(5.0f));
	teapot->SetPosition(0.0f, 10.0f, 0.0f);
	shapes.push_back(teapot);

	// Create a dot (procedural sphere)
	auto dot = std::make_shared<Dot>();
	dot->SetColor(0.0f, 0.8f, 1.0f, 1.0f); // Cyan dot
	dot->SetSize(10.0f);
	dot->SetPosition(30.0f, 10.0f, 0.0f);
	shapes.push_back(dot);

	auto glitter_dot = std::make_shared<Dot>();
	glitter_dot->SetColor(1.0f, 0.0f, 1.0f, 1.0f); // Magenta dot
	glitter_dot->SetSize(8.0f);
	glitter_dot->SetPosition(-30.0f, 10.0f, 0.0f);
	shapes.push_back(glitter_dot);

	visualizer.AddShapeHandler([&](float t) { return shapes; });

	visualizer.AddInputCallback([&](const InputState& state) {
		if (state.key_down[GLFW_KEY_1] && !teapot->IsHidden()) {
			std::cout << "Exploding Teapot with Standard Explosion!" << std::endl;
			visualizer.TriggerComplexExplosion(teapot, glm::vec3(0.0f, 1.0f, 0.0f), 2.0f, FireEffectStyle::Explosion);
		}
		if (state.key_down[GLFW_KEY_2] && !dot->IsHidden()) {
			std::cout << "Exploding Cyan Dot with Sparks!" << std::endl;
			visualizer.TriggerComplexExplosion(dot, glm::vec3(1.0f, 0.5f, 0.0f), 1.5f, FireEffectStyle::Sparks);
		}
		if (state.key_down[GLFW_KEY_3] && !glitter_dot->IsHidden()) {
			std::cout << "Exploding Magenta Dot with GLITTER!" << std::endl;
			visualizer
				.TriggerComplexExplosion(glitter_dot, glm::vec3(-1.0f, 1.0f, 0.0f), 3.0f, FireEffectStyle::Glitter);
		}
		if (state.key_down[GLFW_KEY_R]) {
			std::cout << "Resetting shapes..." << std::endl;
			shapes.clear();

			teapot = std::make_shared<Model>("assets/utah_teapot.obj");
			teapot->SetColor(1.0f, 0.5f, 0.0f, 1.0f);
			teapot->SetScale(glm::vec3(5.0f));
			teapot->SetPosition(0.0f, 10.0f, 0.0f);
			shapes.push_back(teapot);

			dot = std::make_shared<Dot>();
			dot->SetColor(0.0f, 0.8f, 1.0f, 1.0f);
			dot->SetSize(10.0f);
			dot->SetPosition(30.0f, 10.0f, 0.0f);
			shapes.push_back(dot);

			glitter_dot = std::make_shared<Dot>();
			glitter_dot->SetColor(1.0f, 0.0f, 1.0f, 1.0f);
			glitter_dot->SetSize(8.0f);
			glitter_dot->SetPosition(-30.0f, 10.0f, 0.0f);
			shapes.push_back(glitter_dot);
		}
	});

	// Set a good camera position
	Camera cam;
	cam.x = 0.0f;
	cam.y = 40.0f;
	cam.z = 100.0f;
	cam.pitch = -20.0f;
	cam.yaw = 0.0f;
	visualizer.SetCamera(cam);

	std::cout << "Controls:" << std::endl;
	std::cout << "  1: Explode Teapot (Standard)" << std::endl;
	std::cout << "  2: Explode Cyan Dot (Sparks)" << std::endl;
	std::cout << "  3: Explode Magenta Dot (Glitter)" << std::endl;
	std::cout << "  R: Reset shapes" << std::endl;

	visualizer.Run();

	return 0;
}
