#include <iostream>
#include <memory>
#include <vector>

#include "dot.h"
#include "graphics.h"

int main() {
	try {
		Boidsish::Visualizer vis(1280, 720, "PBR Materials Demo");
		Boidsish::Light      light1;
		light1.position = glm::vec3(0, 15, 0);
		light1.color = glm::vec3(0.1, 0.75, 0.85);
		light1.intensity = 20.0f;
		vis.GetLightManager().AddLight(light1);

		vis.AddShapeHandler([&](float time) {
			std::vector<std::shared_ptr<Boidsish::Shape>> shapes;

			// Create a grid of spheres with varying roughness and metallic values
			int   cols = 5;
			int   rows = 4;
			float spacing = 4.0f;
			float start_x = -(cols - 1) * spacing / 2.0f;
			float start_z = -(rows - 1) * spacing / 2.0f;

			int id = 0;

			// Row 0: Non-metallic with varying roughness (plastic/ceramic look)
			for (int col = 0; col < cols; ++col) {
				float x = start_x + col * spacing;
				float z = start_z + 0 * spacing;
				float roughness = (float)col / (cols - 1);

				auto sphere = std::make_shared<Boidsish::Dot>(id++, x, 2.0f, z, 20);
				sphere->SetColor(0.8f, 0.1f, 0.1f); // Red base color
				sphere->SetUsePBR(true);
				sphere->SetRoughness(roughness);
				sphere->SetMetallic(0.0f);
				shapes.push_back(sphere);
			}

			// Row 1: Slightly metallic with varying roughness
			for (int col = 0; col < cols; ++col) {
				float x = start_x + col * spacing;
				float z = start_z + 1 * spacing;
				float roughness = (float)col / (cols - 1);

				auto sphere = std::make_shared<Boidsish::Dot>(id++, x, 2.0f, z, 20);
				sphere->SetColor(0.1f, 0.8f, 0.1f); // Green base color
				sphere->SetUsePBR(true);
				sphere->SetRoughness(roughness);
				sphere->SetMetallic(0.5f);
				shapes.push_back(sphere);
			}

			// Row 2: Fully metallic with varying roughness (polished to brushed metal)
			for (int col = 0; col < cols; ++col) {
				float x = start_x + col * spacing;
				float z = start_z + 2 * spacing;
				float roughness = (float)col / (cols - 1);

				auto sphere = std::make_shared<Boidsish::Dot>(id++, x, 2.0f, z, 20);
				sphere->SetColor(0.1f, 0.1f, 0.8f); // Blue base color
				sphere->SetUsePBR(true);
				sphere->SetRoughness(roughness);
				sphere->SetMetallic(1.0f);
				shapes.push_back(sphere);
			}

			// Row 3: Gold-like material with varying roughness
			for (int col = 0; col < cols; ++col) {
				float x = start_x + col * spacing;
				float z = start_z + 3 * spacing;
				float roughness = (float)col / (cols - 1);

				auto sphere = std::make_shared<Boidsish::Dot>(id++, x, 2.0f, z, 20);
				sphere->SetColor(1.0f, 0.85f, 0.0f); // Gold color
				sphere->SetUsePBR(true);
				sphere->SetRoughness(roughness);
				sphere->SetMetallic(1.0f);
				shapes.push_back(sphere);
			}

			// Add one non-PBR sphere for comparison
			auto legacy_sphere = std::make_shared<Boidsish::Dot>(id++, 0.0f, 6.0f, 0.0f, 15);
			legacy_sphere->SetColor(1.0f, 1.0f, 1.0f); // White
			legacy_sphere->SetUsePBR(false);           // Use legacy lighting
			shapes.push_back(legacy_sphere);

			return shapes;
		});

		vis.Run();
	} catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}
	return 0;
}
