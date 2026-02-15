#include "entity.h"

#include <algorithm>
#include <cmath>

#include "path.h"
#include "terrain_generator.h"
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
				entity->rigid_body_.SetOrientation(
					glm::slerp(entity->rigid_body_.GetOrientation(), update.orientation, 0.1f)
				);
				entity->path_direction_ = update.new_direction;
				entity->path_segment_index_ = update.new_segment_index;
				entity->path_t_ = update.new_t;
			}
		});

		// Call post-timestep hook
		PostTimestep(time, delta_time);

		// Process main thread requests (Visualizer actions)
		// We do this BEFORE processing modification requests (removals)
		// to ensure any actions enqueued by entities that are about to be removed
		// can still access the entity state if needed.
		{
			std::lock_guard<std::mutex> lock(visualizer_mutex_);
			for (auto& request : post_frame_requests_) {
				request();
			}
			post_frame_requests_.clear();
		}

		// Process modification requests (Add/Remove Entity)
		{
			std::lock_guard<std::mutex> lock(requests_mutex_);
			for (auto& request : modification_requests_) {
				request();
			}
			modification_requests_.clear();
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

				// Apply orientation constraint (soft limit)
				if (entity->constraint_orientation_strength_ > 0.0f) {
					glm::vec3 tangent = entity->constraint_path_->GetTangentAtClosestPoint(entity->GetPosition());
					// Use current up vector if possible, or world up
					glm::vec3 up = entity->ObjectToWorld(glm::vec3(0, 1, 0));
					if (glm::length(glm::cross(tangent, up)) < 0.001f) {
						up = glm::vec3(0, 1, 0);
						if (glm::length(glm::cross(tangent, up)) < 0.001f) {
							up = glm::vec3(1, 0, 0);
						}
					}
					glm::quat target_orientation = glm::quatLookAt(tangent, up);
					entity->rigid_body_.SetOrientation(glm::slerp(
						entity->rigid_body_.GetOrientation(),
						target_orientation,
						std::min(1.0f, entity->constraint_orientation_strength_ * delta_time)
					));
				}
			}

			// Update the entity's shape
			entity->UpdateShape();

			// Call the OnEntityUpdated hook
			OnEntityUpdated(entity);
		}

		return {};
	}

	std::tuple<float, glm::vec3> EntityHandler::CalculateTerrainPropertiesAtPoint(float x, float y) const {
		if (vis) {
			return vis->CalculateTerrainPropertiesAtPoint(x, y);
		}
		return {0.0f, glm::vec3(0, 1, 0)};
	}

	const std::vector<std::shared_ptr<Terrain>>& EntityHandler::GetTerrainChunks() const {
		return vis->GetTerrainChunks();
	}

	const TerrainGenerator* EntityHandler::GetTerrainGenerator() const {
		return dynamic_cast<const TerrainGenerator*>(vis->GetTerrain().get());
	}

	// ========== Cache-Preferring Terrain Query Implementations ==========

	std::tuple<float, glm::vec3> EntityHandler::GetTerrainPropertiesAtPoint(float x, float z) const {
		if (vis) {
			return vis->GetTerrainPropertiesAtPoint(x, z);
		}
		return {0.0f, glm::vec3(0, 1, 0)};
	}

	bool EntityHandler::IsPointBelowTerrain(const glm::vec3& point) const {
		if (auto* gen = GetTerrainGenerator()) {
			return gen->IsPointBelowTerrain(point);
		}
		return false;
	}

	float EntityHandler::GetDistanceAboveTerrain(const glm::vec3& point) const {
		if (auto* gen = GetTerrainGenerator()) {
			return gen->GetDistanceAboveTerrain(point);
		}
		return point.y; // Assume terrain at y=0 if no generator
	}

	std::tuple<float, glm::vec3> EntityHandler::GetClosestTerrainInfo(const glm::vec3& point) const {
		if (auto* gen = GetTerrainGenerator()) {
			return gen->GetClosestTerrainInfo(point);
		}
		// Default: assume flat terrain at y=0
		float     dist = std::abs(point.y);
		glm::vec3 dir = point.y > 0 ? glm::vec3(0, -1, 0) : glm::vec3(0, 1, 0);
		return {dist, dir};
	}

	bool EntityHandler::RaycastTerrain(
		const glm::vec3& origin,
		const glm::vec3& direction,
		float            max_distance,
		float&           out_distance,
		glm::vec3&       out_normal
	) const {
		if (auto* gen = GetTerrainGenerator()) {
			return gen->RaycastCached(origin, direction, max_distance, out_distance, out_normal);
		}
		return false;
	}

	bool EntityHandler::IsTerrainCached(float x, float z) const {
		if (auto* gen = GetTerrainGenerator()) {
			return gen->IsPositionCached(x, z);
		}
		return false;
	}

	glm::vec3 EntityHandler::GetValidPlacement(const glm::vec3& suggested_pos, float clearance) const {
		float terrain_h = 0.0f;
		if (vis) {
			auto [h, norm] = vis->GetTerrainPropertiesAtPoint(suggested_pos.x, suggested_pos.z);
			terrain_h = h;
		}
		float min_y = std::max(0.0f, terrain_h) + clearance;
		return glm::vec3(suggested_pos.x, std::max(suggested_pos.y, min_y), suggested_pos.z);
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
