#include <iostream>
#include <memory>
#include <vector>

#include "dot.h"
#include "graphics.h"
#include "spatial_entity_handler.h"
#include "terrain_field.h"

using namespace Boidsish;

class DotEntity: public Entity<> {
public:
	DotEntity(int id, const Vector3& start_pos): Entity<>(id) {
		SetPosition(start_pos);
		SetSize(10.0f);
	}
	void UpdateEntity(EntityHandler&, float, float) override {}
};

class TerrainDemoHandler: public SpatialEntityHandler {
public:
	TerrainDemoHandler() { AddEntity<DotEntity>(Vector3(16.0f, 20.0f, 16.0f)); }

	void PostTimestep(float, float, Visualizer& viz) {
		auto terrain_chunks = viz.getVisibleChunks();
		if (!terrain_chunks.empty()) {
			TerrainField terrain_field(terrain_chunks, 10.0f);
			for (auto const& [id, entity] : GetAllEntities()) {
				Vector3 pos = entity->GetPosition();
				Vector3 influence = terrain_field.getInfluence(pos);
				entity->SetVelocity(influence * 20.0f);
			}
		}
	}
};

int main() {
	try {
		Boidsish::Visualizer visualizer(1280, 720, "Terrain Vector Demo");

		// Set a custom camera position
		Boidsish::Camera camera;
		camera.x = 16.0f;
		camera.y = 25.0f;
		camera.z = 16.0f;
		camera.pitch = -60.0f;
		camera.yaw = -45.0f;
		visualizer.SetCamera(camera);

		// No shapes, just terrain
		TerrainDemoHandler handler;
		visualizer.AddShapeHandler(std::ref(handler));

		visualizer.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
