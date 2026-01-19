#include "entity.h"

#include <algorithm>

#include "path.h"
#include <poolstl/poolstl.hpp>

namespace {
	constexpr float kTurnSpeed = 15.0f;

	// Orients a model to align with its velocity vector.
	// Assumes the model's "forward" direction is -Z.
	glm::quat OrientToVelocity(glm::quat current_orientation, const Boidsish::Vector3& vel, float delta_time) {
		if (vel.MagnitudeSquared() < 1e-6) {
			return current_orientation;
		}

		const glm::vec3 forward_dir = glm::normalize(glm::vec3(vel.x, vel.y, vel.z));
		const glm::vec3 world_up = glm::vec3(0.0f, 1.0f, 0.0f);

		// Create a quaternion that looks along forward_dir.
		// glm::quatLookAt is designed to be robust, even when forward_dir is
		// parallel to world_up. It assumes the "forward" direction is -Z.
		glm::quat target_orientation = glm::quatLookAt(forward_dir, world_up);

		// Slerp for smooth turning.
		return glm::slerp(current_orientation, target_orientation, kTurnSpeed * delta_time);
	}
} // namespace

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
					entity->rigid_body_.GetOrientation(),
					entity->path_segment_index_,
					entity->path_t_,
					entity->path_direction_,
					entity->path_speed_,
					delta_time
				);
				entity->SetVelocity(update.velocity * entity->path_speed_);
				entity->rigid_body_.SetOrientation(glm::slerp(entity->rigid_body_.GetOrientation(), update.orientation, 0.1f));
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

		// Process main thread requests
		{
			std::lock_guard<std::mutex> lock(visualizer_mutex_);
			for (auto& request : post_frame_requests_) {
				request();
			}
			post_frame_requests_.clear();
		}

		// Generate shapes from entity states
		for (auto& entity : entities) {
			// Orient to velocity
			if (entity->orient_to_velocity_) {
				entity->rigid_body_.FaceVelocity();
			}

			entity->rigid_body_.Update(delta_time);

			// Apply path constraint
			if (entity->constraint_path_) {
				glm::vec3 closest_point_glm = entity->constraint_path_->FindClosestPoint(entity->GetPosition());
				Vector3   closest_point(closest_point_glm.x, closest_point_glm.y, closest_point_glm.z);
				Vector3   to_path = closest_point - entity->GetPosition();
				float     distance_from_path = to_path.Magnitude();

				if (distance_from_path > entity->constraint_radius_) {
					Vector3 from_path = to_path.Normalized() * -1.0f;
					Vector3 corrected_position = closest_point + from_path * entity->constraint_radius_;
					entity->SetPosition(corrected_position);
				}
			}

			// Update the entity's shape
			entity->UpdateShape();

			// Call the OnEntityUpdated hook
			OnEntityUpdated(entity);
		}

		return {};
	}

	std::tuple<float, glm::vec3> EntityHandler::GetTerrainPointProperties(float x, float y) const {
		return vis->GetTerrainPointProperties(x, y);
	}

	const std::vector<std::shared_ptr<Terrain>>& EntityHandler::GetTerrainChunks() const {
		return vis->GetTerrainChunks();
	}

	const ITerrainGenerator* EntityHandler::GetTerrainGenerator() const {
		return vis->GetTerrainGenerator();
	}

	EntityHandler::~EntityHandler() {
		std::lock_guard<std::mutex> lock(requests_mutex_);
		for (auto& request : modification_requests_) {
			request();
		}
		modification_requests_.clear();

		std::lock_guard<std::mutex> lock2(visualizer_mutex_);
		for (auto& request : post_frame_requests_) {
			request();
		}
		post_frame_requests_.clear();
	}
} // namespace Boidsish
