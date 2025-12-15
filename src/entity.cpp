#include "entity.h"

namespace Boidsish {

	const std::vector<std::shared_ptr<Shape>>& EntityHandler::GetShapes(float time) {
		float delta_time = (last_time_ >= 0) ? time - last_time_ : 0.0f;
		last_time_ = time;

		PreTimestep(time, delta_time);

		// Update all entities
		for (auto& pair : entities_) {
			pair.second->UpdateEntity(*this, time, delta_time);
		}

		PostTimestep(time, delta_time);

		// Integrate positions
		for (auto& pair : entities_) {
			auto& entity = pair.second;
			entity->SetPosition(entity->GetPosition() + entity->GetVelocity() * delta_time);
			entity->UpdateShape();
		}

		// Collect shapes for rendering
		shapes_.clear();
		for (auto& pair : entities_) {
			shapes_.push_back(pair.second->GetShape());
		}

		return shapes_;
	}

} // namespace Boidsish
