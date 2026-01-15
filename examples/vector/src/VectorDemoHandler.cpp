#include "VectorDemoHandler.h"

#include <algorithm> // For std::clamp
#include <iostream>

#include "FlockingEntity.h"
#include "FruitEntity.h"
#include "VectorDemoEntity.h"
#include "graphics.h"

namespace Boidsish {

	VectorDemoHandler::VectorDemoHandler(task_thread_pool::task_thread_pool& thread_pool, Visualizer& viz):
		SpatialEntityHandler(thread_pool), eng(rd()), viz_(viz) {
		std::cout << "=== Vector3 Operations Demo ===" << std::endl;

		// Create some vector demo entities
		for (int i = 0; i < 8; i++) {
			Vector3 start_pos(10 * sin(i / 4.0f), 1.0f, 10 * cos(i / 6.0f));
			this->AddEntity<VectorDemoEntity>(start_pos);
		}

		// Create a flock of entities
		for (int i = 0; i < 64; i++) {
			Vector3 start_pos((rand() % 10 - 5) * 2.0f, -1, (rand() % 10 - 5) * 2.0f);
			this->AddEntity<FlockingEntity>(start_pos);
		}

		for (int i = 0; i < 16; i++) {
			this->AddEntity<FruitEntity>();
		}

		std::cout << "Created 32 flocking entities and 4 target-seeking entities" << std::endl;
		std::cout << "Demonstrating Vector3 operations: addition, subtraction, normalization," << std::endl;
		std::cout << "dot product, cross product, magnitude, and distance calculations!" << std::endl;
		std::cout << "Flocking entities now automatically discover each other through the handler!" << std::endl;
	}

	void VectorDemoHandler::AddEntity(int id, std::shared_ptr<EntityBase> entity) {
		SpatialEntityHandler::AddEntity(id, entity);
		// EnqueueVisualizerAction([this, entity]() {
		entity->UpdateShape();
		viz_.AddShape(entity->GetShape());
		// });
	}

	void VectorDemoHandler::RemoveEntity(int id) {
		SpatialEntityHandler::RemoveEntity(id);
		// EnqueueVisualizerAction([this, id]() {
		viz_.RemoveShape(id);
		// });
	}

	void VectorDemoHandler::PreTimestep(float time, float delta_time) {
		(void)time;
		float fruitRate = 2.0f;
		auto  numFlocker = GetEntitiesByType<FlockingEntity>().size();
		if (numFlocker <= 4) {
			Vector3 start_pos((rand() % 10 - 5) * 2.0f, (rand() % 6 - 3) * 2.0f, (rand() % 10 - 5) * 2.0f);
			this->AddEntity<FlockingEntity>(start_pos);
			fruitRate++;
		} else if (numFlocker > 96) {
			fruitRate--;
		}

		fruitRate = std::max(4.0f, fruitRate);

		if (GetEntitiesByType<VectorDemoEntity>().size() < 1) {
			Vector3 start_pos(10 * sin(rand() / 4.0f), 1.0f, 10 * cos(rand() / 6.0f));
			this->AddEntity<VectorDemoEntity>(start_pos);
		}

		float weightedOdds = delta_time * fruitRate *
			std::clamp(1.0f - (GetEntitiesByType<FruitEntity>().size() / 32.0f), 0.0f, 1.0f);
		std::bernoulli_distribution dist(weightedOdds);
		bool                        makeFruit = dist(eng);
		if (makeFruit) {
			this->AddEntity<FruitEntity>();
		}
	}

} // namespace Boidsish
