#include <iostream>
#include <memory>
#include <vector>

#include "dot.h"
#include "graphics.h"
#include "light.h"

int main() {
	try {
		Boidsish::Visualizer vis(1280, 720, "Multi-Light Demo");

		// Add colored lights - PBR intensity boost is handled in shader
		Boidsish::Light light1;
		light1.position = glm::vec3(5, 5, 5);
		light1.color = glm::vec3(1, 0, 0);
		light1.intensity = 2.0f;
		vis.GetLightManager().AddLight(light1);

		Boidsish::Light light2;
		light2.position = glm::vec3(-5, 5, 5);
		light2.color = glm::vec3(0, 1, 0);
		light2.intensity = 3.0f;
		vis.GetLightManager().AddLight(light2);

		Boidsish::Light light3;
		light3.position = glm::vec3(0, 5, -5);
		light3.color = glm::vec3(0, 0, 1);
		light3.intensity = 2.0f;
		vis.GetLightManager().AddLight(light3);

		vis.AddShapeHandler([&](float) {
			std::vector<std::shared_ptr<Boidsish::Shape>> shapes;

			// Central sphere with PBR - polished metal
			auto sphere = std::make_shared<Boidsish::Dot>(0, 0, 10, 0, 20, 1.0f, 1.0f, 1.0f);
			sphere->SetUsePBR(true);
			sphere->SetRoughness(0.2f); // Quite smooth/shiny
			sphere->SetMetallic(1.0f);  // Fully metallic
			shapes.push_back(sphere);

			// Add some orbiting spheres with different materials
			float radius = 5.0f;
			for (int i = 0; i < 4; ++i) {
				float angle = (float)i / 4.0f * 2.0f * 3.14159f;
				float x = cos(angle) * radius;
				float z = sin(angle) * radius;

				auto orb = std::make_shared<Boidsish::Dot>(i + 1, x, 0, z, 10);
				orb->SetColor(0.8f, 0.6f, 0.2f);
				orb->SetUsePBR(true);
				orb->SetRoughness(0.3f + i * 0.2f);         // Varying roughness
				orb->SetMetallic(i % 2 == 0 ? 0.0f : 0.8f); // Alternate metallic/non-metallic
				shapes.push_back(orb);
			}

			return shapes;
		});

		vis.Run();
	} catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}
	return 0;
}
