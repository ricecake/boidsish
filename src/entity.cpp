#include "entity.h"

#include <algorithm>

#include "path.h"
#include <poolstl/poolstl.hpp>

namespace {
	constexpr float kTurnSpeed = 15.0f;
}

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
				auto update = entity->path_->CalculateUpdate(
					entity->GetPosition(),
					entity->orientation_,
					entity->path_segment_index_,
					entity->path_t_,
					entity->path_direction_,
					entity->path_speed_,
					delta_time
				);
				entity->SetVelocity(update.velocity * entity->path_speed_);
				entity->orientation_ = glm::slerp(entity->orientation_, update.orientation, 0.1f);
				entity->path_direction_ = update.new_direction;
				entity->path_segment_index_ = update.new_segment_index;
				entity->path_t_ = update.new_t;
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
			// Orient to velocity
			if (entity->orient_to_velocity_) {
				const auto& vel = entity->GetVelocity();
				if (vel.MagnitudeSquared() > 1e-6) {
					glm::vec3 forward(0.0f, 1.0f, 0.0f); // Default "forward" for Arrow
					glm::vec3 norm_vel = glm::normalize(glm::vec3(vel.x, vel.y, vel.z));

					float     dot = glm::dot(forward, norm_vel);
					glm::quat target_rot;

					if (std::abs(dot - (-1.0f)) < 1e-6) {
						// Opposite direction, 180 degree turn
						target_rot = glm::angleAxis(glm::pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
					} else {
						glm::vec3 rotAxis = glm::cross(forward, norm_vel);
						float     rotAngle = std::acos(dot);
						target_rot = glm::angleAxis(rotAngle, rotAxis);
					}

					// Smoothly interpolate to the target rotation
					entity->orientation_ = glm::slerp(entity->orientation_, target_rot, kTurnSpeed * delta_time);
				}
			}

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
