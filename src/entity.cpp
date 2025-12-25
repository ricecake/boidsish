#include "entity.h"

#include <algorithm>

#include <poolstl/poolstl.hpp>
#include "path.h"

namespace Boidsish {
	std::vector<std::shared_ptr<Shape>> EntityHandler::operator()(float time) {
		float delta_time = 0.016f; // Default 60 FPS
		if (last_time_ >= 0.0f) {
			delta_time = time - last_time_;
		}
		last_time_ = time;

		// Call pre-timestep hook
		PreTimestep(time, delta_time);

		// Get entities
		std::vector<std::shared_ptr<EntityBase>> entities;
		std::transform(entities_.begin(), entities_.end(), std::back_inserter(entities), [](const auto& pair) {
			return pair.second;
		});

		// Update all entities
		std::for_each(poolstl::par.on(thread_pool_), entities.begin(), entities.end(), [&](auto& entity) {
			entity->UpdateEntity(*this, time, delta_time);
			if (entity->path_) {
				auto update = entity->path_->CalculateUpdate(entity->GetPosition(), entity->orientation_, entity->path_direction_, delta_time);
				entity->SetVelocity(update.velocity * entity->path_speed_);
				entity->orientation_ = glm::slerp(entity->orientation_, update.orientation, 0.1f);
				entity->path_direction_ = update.new_direction;
			}
		});

		// Call post-timestep hook
		PostTimestep(time, delta_time);

		// Process modification requests
		{
			std::lock_guard<std::mutex> lock(requests_mutex_);
			for (auto& request : modification_requests_) {
				request();
			}
			modification_requests_.clear();
		}

		// Generate shapes from entity states
		std::vector<std::shared_ptr<Shape>> shapes;
		shapes.reserve(entities_.size());

		for (auto& entity : entities) {
			// Update entity position using its velocity
			Vector3 new_position = entity->GetPosition() + entity->GetVelocity() * delta_time;
			entity->SetPosition(new_position);

			// Update the entity's shape
			entity->UpdateShape();

			// Add shape to the list
			shapes.push_back(entity->GetShape());
		}

		return shapes;
	}

	const auto EntityHandler::GetTerrainPointProperties(float x, float y) {

	};
	const auto EntityHandler::GetTerrainChunks() {

	};

} // namespace Boidsish
