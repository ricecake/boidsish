#include <iostream>
#include <memory>
#include <vector>

#include "graphics.h"
#include "procedural_generator.h"

using namespace Boidsish;

std::vector<std::shared_ptr<Model>> city_models;

void PrepareCity(Visualizer& viz) {
	for (int x = -2; x <= 2; ++x) {
		for (int z = -2; z <= 2; ++z) {
			float world_x = x * 15.0f;
			float world_z = z * 15.0f;

			// Sample terrain height to place building correctly
			auto [terrain_height, terrain_normal] = viz.GetTerrainPropertiesAtPoint(world_x, world_z);

			unsigned int seed = (x + 10) * 100 + (z + 10);
			auto building = ProceduralGenerator::Generate(ProceduralType::Structure, seed);
			building->SetPosition(world_x, terrain_height, world_z);

			// Level the terrain under the building
			ProceduralGenerator::LevelTerrainForModel(viz, building);

			viz.AddShape(building);
			city_models.push_back(building);
		}
	}
}

int main() {
	try {
		Visualizer viz(1280, 720, "Procedural City Demo");

		viz.AddPrepareCallback(PrepareCity);

		Camera cam;
		cam.x = 0;
		cam.y = 15;
		cam.z = 40;
		cam.pitch = -20;
		viz.SetCamera(cam);

		// Speed up day/night cycle to see the windows light up
		viz.GetLightManager().GetDayNightCycle().speed = 1.0f;

		viz.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
