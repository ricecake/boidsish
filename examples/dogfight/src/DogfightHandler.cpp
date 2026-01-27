#include "DogfightHandler.h"
#include "terrain_generator.h"

namespace Boidsish {

	DogfightHandler::DogfightHandler(
		task_thread_pool::task_thread_pool& thread_pool,
		std::shared_ptr<Visualizer>         visualizer
	):
		SpatialEntityHandler(thread_pool, visualizer) {}

	void DogfightHandler::PreTimestep(float time, float delta_time) {
		(void)time;
		(void)delta_time;

		int red_count = 0;
		int blue_count = 0;

		for (auto& pair : GetAllEntities()) {
			auto plane = std::dynamic_pointer_cast<DogfightPlane>(pair.second);
			if (plane) {
				if (plane->GetTeam() == Team::RED)
					red_count++;
				else
					blue_count++;
			}
		}

		const int kMinPlanes = 15;
		if (red_count < kMinPlanes)
			SpawnPlane(Team::RED);
		if (blue_count < kMinPlanes)
			SpawnPlane(Team::BLUE);
	}

	void DogfightHandler::SpawnPlane(Team team) {
		float x = (float)(rand() % 1000 - 500);
		float z = (float)(rand() % 1000 - 500);

		// We can't easily get terrain height here if generator isn't ready or visible
		// but EntityHandler has GetTerrainPointPropertiesThreadSafe
		auto [h, n] = GetTerrainPointPropertiesThreadSafe(x, z);

		QueueAddEntity<DogfightPlane>(team, Vector3(x, h + 150.0f, z));
	}

} // namespace Boidsish
