#include <vector>

#include "dot.h"
#include "graphics.h"
#include "path.h"
#include "terrain_generator.h"

using namespace Boidsish;

int main(int argc, char** argv) {
	Boidsish::Visualizer visualizer(1024, 768, "Terrain Path Demo");

	// Get the terrain generator from the visualizer
	const Boidsish::TerrainGenerator* terrain_generator = visualizer.GetTerrainGenerator();

		// Get the path
		auto terrainPath = terrain_generator->GetPath({0, 0}, 200, 5.0f);
		auto path_handler = std::make_shared<PathHandler>();
		auto path = path_handler->AddPath();
		path->SetMode(PathMode::LOOP);
		std::vector<std::shared_ptr<Boidsish::Shape>> shapes;
		for (const auto& point : terrainPath) {
			path->AddWaypoint({point.x, point.y+10, point.z});
		}

		// Create a shape handler to render the path
		visualizer.AddShapeHandler([&](float time) { return path_handler->GetShapes(); });

		visualizer.SetPathCamera(path);

	visualizer.Run();

	return 0;
}
