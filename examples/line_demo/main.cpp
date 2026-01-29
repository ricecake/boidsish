#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "graphics.h"
#include "line.h"
#include "vector.h"

using namespace Boidsish;

std::vector<std::shared_ptr<Shape>> LineDemoHandler(float time) {
	std::vector<std::shared_ptr<Shape>> shapes;

	// 1. A simple solid line
	auto solidLine = std::make_shared<Line>(glm::vec3(-10, 0, 0), glm::vec3(-5, 5, 0), 0.2f);
	solidLine->SetColor(1.0f, 0.0f, 0.0f, 1.0f);
	shapes.push_back(solidLine);

	// 2. A stylized laser (green) - Thicker to see glow
	auto laser1 = std::make_shared<Line>(glm::vec3(-2, 2, 0), glm::vec3(8, 2, 0), 0.5f);
	laser1->SetColor(0.0f, 1.0f, 0.0f, 1.0f);
	laser1->SetStyle(Line::Style::LASER);
	shapes.push_back(laser1);

	// 3. A stylized laser (blue) - Very thick
	auto laser2 = std::make_shared<Line>(glm::vec3(-5, 5, -5), glm::vec3(-5, -5, 5), 1.0f);
	laser2->SetColor(0.0f, 0.5f, 1.0f, 1.0f);
	laser2->SetStyle(Line::Style::LASER);
	shapes.push_back(laser2);

	// 4. A dynamic laser (yellow)
	auto laser3 = std::make_shared<Line>(
		glm::vec3(0, 0, 0),
		glm::vec3(5.0f * std::sin(time), 10.0f, 5.0f * std::cos(time)),
		0.3f
	);
	laser3->SetColor(1.0f, 1.0f, 0.0f, 1.0f);
	laser3->SetStyle(Line::Style::LASER);
	shapes.push_back(laser3);

	return shapes;
}

int main() {
	try {
		Visualizer visualizer(1280, 720, "Line Subclass Demo");

		// Setup camera
		Camera cam;
		cam.x = 0;
		cam.y = 5;
		cam.z = 20;
		cam.yaw = 0;
		cam.pitch = -10;
		visualizer.SetCamera(cam);

		// Add some lights
		visualizer.GetLightManager().AddLight(Light::Create(glm::vec3(5, 10, 5), 1.0f, glm::vec3(1, 1, 1)));
		visualizer.GetLightManager().SetAmbientLight(glm::vec3(0.1f, 0.1f, 0.1f));

		// Set the shape handler
		visualizer.AddShapeHandler(LineDemoHandler);

		std::cout << "Line Subclass Demo Started!" << std::endl;
		std::cout << "Press ESC to exit." << std::endl;

		// Run the visualization
		visualizer.Run();

		std::cout << "Visualization ended." << std::endl;

	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
