#include <iostream>
#include <memory>
#include <vector>
#include <cmath>

#include "dot.h"
#include "polyhedron.h"
#include "graphics.h"

int main() {
	try {
		Boidsish::Visualizer vis(1280, 720, "Linearly Transformed Cosines (LTC) Area Light Demo");

		// Disable day/night cycle to focus purely on our neon area lights
		vis.GetLightManager().GetDayNightCycle().paused = true;
		vis.GetLightManager().GetDayNightCycle().time = 12.0f; // noon/flat ambient
		vis.GetLightManager().SetAmbientLight(glm::vec3(0.02f, 0.02f, 0.05f)); // Dark environment for high neon contrast

		vis.AddShapeHandler([&](float time) {
			std::vector<std::shared_ptr<Boidsish::Shape>> shapes;

			// Add a central rotating neon area light representation
			// We can animate its orientation
			float angle = time * 0.5f;
			glm::vec3 light_pos = glm::vec3(0.0f, 6.0f, 0.0f);
			glm::vec3 light_normal = glm::vec3(sin(angle) * 0.2f, -1.0f, cos(angle) * 0.2f);
			glm::vec3 light_right = glm::vec3(cos(angle), 0.0f, -sin(angle));
			glm::vec3 light_color = glm::vec3(1.0f, 0.0f, 0.5f); // Hot Magenta Neon
			float light_width = 8.0f;
			float light_height = 2.0f;
			float light_intensity = 15.0f;

			// Re-create/update the area light on each frame
			vis.GetLightManager().GetLights().clear();

			// Add a second contrasting cyan area light slightly offset
			glm::vec3 light_pos2 = glm::vec3(8.0f, 6.0f, 4.0f);
			glm::vec3 light_normal2 = glm::vec3(0.0f, -1.0f, 0.0f);
			glm::vec3 light_right2 = glm::vec3(1.0f, 0.0f, 0.0f);
			glm::vec3 light_color2 = glm::vec3(0.0f, 0.8f, 1.0f); // Bright Cyan Neon

			vis.GetLightManager().AddLight(Boidsish::Light::CreateArea(
				light_pos, light_normal, light_right, light_width, light_height, light_intensity, light_color
			));
			vis.GetLightManager().AddLight(Boidsish::Light::CreateArea(
				light_pos2, light_normal2, light_right2, 4.0f, 4.0f, 10.0f, light_color2
			));

			// Create a representation of the area lights as glowing flat rectangles
			// We use curvely aligned emissive dots/quads to visualize them
			auto light_quad = std::make_shared<Boidsish::Dot>(-1, light_pos.x, light_pos.y, light_pos.z, 15);
			light_quad->SetColor(light_color.r, light_color.g, light_color.b);
			light_quad->SetScale(glm::vec3(light_width * 0.4f, 1.0f, light_height * 0.4f));
			light_quad->SetUsePBR(true);
			light_quad->SetEmissive(true);
			shapes.push_back(light_quad);

			auto light_quad2 = std::make_shared<Boidsish::Dot>(-2, light_pos2.x, light_pos2.y, light_pos2.z, 15);
			light_quad2->SetColor(light_color2.r, light_color2.g, light_color2.b);
			light_quad2->SetScale(glm::vec3(1.5f));
			light_quad2->SetUsePBR(true);
			light_quad2->SetEmissive(true);
			shapes.push_back(light_quad2);

			// Populate a grid of spheres underneath the area light to show reflections
			int   cols = 7;
			int   rows = 7;
			float spacing = 2.5f;
			float start_x = -(cols - 1) * spacing / 2.0f;
			float start_z = -(rows - 1) * spacing / 2.0f;

			int id = 100;
			for (int row = 0; row < rows; ++row) {
				float z = start_z + row * spacing;
				float metallic = (float)row / (rows - 1); // Metallic increases from top to bottom

				for (int col = 0; col < cols; ++col) {
					float x = start_x + col * spacing;
					float roughness = 0.05f + 0.9f * ((float)col / (cols - 1)); // Roughness increases from left to right

					auto sphere = std::make_shared<Boidsish::Dot>(id++, x, 1.5f, z, 24);
					sphere->SetColor(0.2f, 0.2f, 0.25f); // Neutral dark base color
					sphere->SetUsePBR(true);
					sphere->SetRoughness(roughness);
					sphere->SetMetallic(metallic);
					shapes.push_back(sphere);
				}
			}

			// Add a few shiny neon cuboids/polyhedra for geometric variety
			auto cube1 = std::make_shared<Boidsish::Polyhedron>(Boidsish::PolyhedronType::Cube, id++, -8.0f, 2.0f, -4.0f, 2.0f);
			cube1->SetColor(0.1f, 0.1f, 0.1f);
			cube1->SetUsePBR(true);
			cube1->SetRoughness(0.1f);
			cube1->SetMetallic(1.0f);
			shapes.push_back(cube1);

			auto cube2 = std::make_shared<Boidsish::Polyhedron>(Boidsish::PolyhedronType::Icosahedron, id++, 8.0f, 2.0f, -4.0f, 2.0f);
			cube2->SetColor(0.8f, 0.8f, 0.8f);
			cube2->SetUsePBR(true);
			cube2->SetRoughness(0.2f);
			cube2->SetMetallic(0.0f);
			shapes.push_back(cube2);

			return shapes;
		});

		vis.Run();
	} catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}
	return 0;
}
