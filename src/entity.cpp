#include "entity.h"

namespace Boidsish {
	std::vector<std::shared_ptr<Shape>> EntityHandler::operator()(float time) {
		float delta_time = 0.016f; // Default 60 FPS
		if (last_time_ >= 0.0f) {
			delta_time = time - last_time_;
		}
		last_time_ = time;

		// Get all entities for processing
		std::vector<std::shared_ptr<EntityBase>> entities;
		std::transform(entities_.begin(), entities_.end(), std::back_inserter(entities), [](const auto& pair) {
			return pair.second;
		});

		// First, update all entity positions based on their current velocity
		for (auto& entity : entities) {
			Vector3 new_position = entity->GetPosition() + entity->GetVelocity() * delta_time;
			entity->SetPosition(new_position);
		}

		// Call pre-timestep hook (this is where SpatialEntityHandler will build the R-Tree)
		PreTimestep(time, delta_time);

		// Now, update entity logic (steering, flocking, etc.) which may depend on neighbor positions
		for (auto& entity : entities) {
			entity->UpdateEntity(*this, time, delta_time);
		}

		// Call post-timestep hook (for cleanup, interactions, etc.)
		PostTimestep(time, delta_time);

		// Finally, generate shapes from the updated entity states
		std::vector<std::shared_ptr<Shape>> shapes;
		shapes.reserve(entities.size());
		for (auto& entity : entities) {
			entity->UpdateShape();
			shapes.push_back(entity->GetShape());
		}

		return shapes;
	}
} // namespace Boidsish