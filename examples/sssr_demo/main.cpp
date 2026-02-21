#include <iostream>
#include <memory>
#include <vector>
#include <cmath>

#include "dot.h"
#include "graphics.h"

int main() {
	try {
		Boidsish::Visualizer vis(1280, 720, "SSSR Demonstration Scene");

		// 1. Setup Camera
		auto& cam = vis.GetCamera();
		cam.x = 0.0f;
		cam.y = 15.0f;
		cam.z = 50.0f;
		cam.pitch = -15.0f;
		cam.yaw = 0.0f;

		// 2. Setup Lighting
		// We want some strong lights to see reflections
		vis.GetLightManager().GetLights().clear(); // Clear default lights

		// Directional light (Sun)
		Boidsish::Light sun = Boidsish::Light::CreateDirectional(45.0f, 45.0f, 1.2f, glm::vec3(1.0f, 0.95f, 0.9f), true);
		vis.GetLightManager().AddLight(sun);

		// A few point lights with distinct colors
		Boidsish::Light p1 = Boidsish::Light::Create(glm::vec3(-20, 10, -10), 10.0f, glm::vec3(1, 0.2, 0.2));
		vis.GetLightManager().AddLight(p1);

		Boidsish::Light p2 = Boidsish::Light::Create(glm::vec3(20, 10, -10), 10.0f, glm::vec3(0.2, 0.2, 1));
		vis.GetLightManager().AddLight(p2);

		// 3. Add Shapes
		vis.AddShapeHandler([&](float time) {
			std::vector<std::shared_ptr<Boidsish::Shape>> shapes;

			// Giant mirror sphere in the center
			auto mirror_sphere = std::make_shared<Boidsish::Dot>(100, 0.0f, 8.0f, -20.0f, 60);
			mirror_sphere->SetScale(glm::vec3(8.0f));
			mirror_sphere->SetColor(1.0f, 1.0f, 1.0f);
			mirror_sphere->SetUsePBR(true);
			mirror_sphere->SetRoughness(0.01f);
			mirror_sphere->SetMetallic(1.0f);
			shapes.push_back(mirror_sphere);

			// Orbiting colorful spheres
			int count = 6;
			for (int i = 0; i < count; ++i) {
				float angle = (float)i / (float)count * 2.0f * 3.14159f + time * 0.5f;
				float radius = 25.0f;
				float x = std::cos(angle) * radius;
				float z = std::sin(angle) * radius - 20.0f;

				auto s = std::make_shared<Boidsish::Dot>(i, x, 4.0f, z, 30);
				s->SetScale(glm::vec3(4.0f));

				// Pure primary/secondary colors
				glm::vec3 color;
				if (i == 0) color = glm::vec3(1, 0, 0);
				else if (i == 1) color = glm::vec3(0, 1, 0);
				else if (i == 2) color = glm::vec3(0, 0, 1);
				else if (i == 3) color = glm::vec3(1, 1, 0);
				else if (i == 4) color = glm::vec3(1, 0, 1);
				else color = glm::vec3(0, 1, 1);

				s->SetColor(color.r, color.g, color.b);
				s->SetUsePBR(true);
				s->SetRoughness(0.05f);
				s->SetMetallic(0.2f);
				shapes.push_back(s);
			}

			// High-roughness sphere to test specular lobe spreading
			auto rough_sphere = std::make_shared<Boidsish::Dot>(200, -30.0f, 10.0f, 10.0f, 40);
			rough_sphere->SetScale(glm::vec3(10.0f));
			rough_sphere->SetColor(0.8f, 0.5f, 0.2f);
			rough_sphere->SetUsePBR(true);
			rough_sphere->SetRoughness(0.6f);
			rough_sphere->SetMetallic(0.1f);
			shapes.push_back(rough_sphere);

			return shapes;
		});

		vis.Run();
	} catch (const std::exception& e) {
		std::cerr << "Demo error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
