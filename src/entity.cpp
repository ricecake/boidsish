#include "entity.h"

#include <algorithm>

#include "path.h"
#include <poolstl/poolstl.hpp>

namespace {
	constexpr float kTurnSpeed = 15.0f;

	glm::quat OrientToVelocity(glm::quat current_orientation, const Boidsish::Vector3& vel, float delta_time) {
		if (vel.MagnitudeSquared() < 1e-6) {
			return current_orientation;
		}

		// The Arrow model's "forward" is its local +Y axis.
		// glm::lookAt assumes the "forward" is the local -Z axis.
		// We need a corrective rotation to align +Y with -Z.
		// A -90 degree rotation around the X-axis achieves this.
		glm::quat correction = glm::angleAxis(-glm::pi<float>() / 2.0f, glm::vec3(1.0f, 0.0f, 0.0f));

		glm::vec3 forward = glm::normalize(glm::vec3(vel.x, vel.y, vel.z));
		glm::vec3 world_up = glm::vec3(0.0f, 1.0f, 0.0f);

		glm::quat target_orientation;

		// Handle the edge case where the forward vector is parallel to the world up vector.
		if (glm::abs(glm::dot(forward, world_up)) > 0.999f) {
			// If facing straight up or down, we need a different temporary up vector
			// to calculate the right vector. Let's use the world's X-axis.
			glm::vec3 temp_up = glm::vec3(1.0f, 0.0f, 0.0f);

			// If we're also aligned with the world X-axis, use Z-axis.
			if (glm::abs(glm::dot(forward, temp_up)) > 0.999f) {
				temp_up = glm::vec3(0.0f, 0.0f, 1.0f);
			}

			glm::vec3 right = glm::normalize(glm::cross(forward, temp_up));
			glm::vec3 up = glm::normalize(glm::cross(right, forward));
			glm::mat4 orientation_matrix = glm::mat4(
				glm::vec4(right, 0.0f),
				glm::vec4(up, 0.0f),
				glm::vec4(-forward, 0.0f), // Negated forward for a right-handed system
				glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)
			);
			target_orientation = glm::quat_cast(orientation_matrix);
		} else {
			// The standard case, using glm::lookAt
			glm::mat4 look_at = glm::lookAt(glm::vec3(0.0f), forward, world_up);
			target_orientation = glm::quat_cast(glm::inverse(look_at));
		}

		// Apply the correction and slerp
		return glm::slerp(current_orientation, target_orientation * correction, kTurnSpeed * delta_time);
	}
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
				entity->orientation_ = OrientToVelocity(entity->orientation_, entity->GetVelocity(), delta_time);
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
