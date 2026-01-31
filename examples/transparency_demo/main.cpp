#include <iostream>
#include <memory>
#include <vector>

#include "dot.h"
#include "graphics.h"

int main() {
	try {
		Boidsish::Visualizer vis(1280, 720, "Transparency Demo");

		// Add a bright light to see specular highlights
		Boidsish::Light light1;
		light1.position = glm::vec3(5, 10, 5);
		light1.color = glm::vec3(1.0, 1.0, 1.0);
		light1.intensity = 15.0f;
		vis.GetLightManager().AddLight(light1);

		// Add another light from the side
		Boidsish::Light light2;
		light2.position = glm::vec3(-10, 5, 0);
		light2.color = glm::vec3(0.5, 0.7, 1.0);
		light2.intensity = 10.0f;
		vis.GetLightManager().AddLight(light2);

		vis.AddShapeHandler([&](float time) {
			std::vector<std::shared_ptr<Boidsish::Shape>> shapes;

			// 1. Opaque PBR Sphere
			auto opaque = std::make_shared<Boidsish::Dot>(0, -6.0f, 2.0f, 0.0f, 30.0f);
			opaque->SetColor(0.8f, 0.1f, 0.1f, 1.0f);
			opaque->SetUsePBR(true);
			opaque->SetRoughness(0.2f);
			opaque->SetMetallic(0.0f);
			shapes.push_back(opaque);

			// 2. Translucent PBR Sphere (Glass-like)
			auto glass = std::make_shared<Boidsish::Dot>(1, 0.0f, 2.0f, 0.0f, 30.0f);
			glass->SetColor(0.1f, 0.4f, 0.8f, 0.2f); // Low alpha
			glass->SetUsePBR(true);
			glass->SetRoughness(0.05f); // Very smooth
			glass->SetMetallic(0.0f);
			shapes.push_back(glass);

			// 3. Nearly Transparent PBR Sphere
			auto ghost = std::make_shared<Boidsish::Dot>(2, 6.0f, 2.0f, 0.0f, 30.0f);
			ghost->SetColor(1.0f, 1.0f, 1.0f, 0.05f); // Very low alpha
			ghost->SetUsePBR(true);
			ghost->SetRoughness(0.1f);
			ghost->SetMetallic(0.0f);
			shapes.push_back(ghost);

			return shapes;
		});

		vis.Run();
	} catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}
	return 0;
}
