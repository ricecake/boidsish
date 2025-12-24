#include "entity.h"
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
		for (auto& entity : entities) {
			entity->UpdateEntity(*this, time, delta_time);
			if (entity->path_) {
				auto update = entity->path_->CalculateUpdate(entity->GetPosition(), entity->orientation_, delta_time);
				entity->SetVelocity(update.first * entity->path_speed_);
				entity->orientation_ = glm::slerp(entity->orientation_, update.second, 0.1f);
			}
		}

		// Call post-timestep hook
		PostTimestep(time, delta_time);

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
