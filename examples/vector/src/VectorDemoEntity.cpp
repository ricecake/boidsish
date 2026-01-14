#include "VectorDemoEntity.h"

#include "FlockingEntity.h"
#include "logger.h"
#include "spatial_entity_handler.h"

namespace Boidsish {

	VectorDemoEntity::VectorDemoEntity(int id, const Vector3& start_pos): Entity<>(id), phase_(0.0f) {
		SetPosition(start_pos);
		SetSize(10.0f);
		SetTrailLength(100);
		SetTrailIridescence(false);
	}

	void VectorDemoEntity::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		(void)time; // Mark unused parameters
		phase_ += delta_time;

		auto& spatial_handler = static_cast<const SpatialEntityHandler&>(handler);
		auto  current_pos = GetPosition();
		auto  targetInstance = std::static_pointer_cast<FlockingEntity>(handler.GetEntity(target_id));

		if (targetInstance != nullptr) {
			auto target = targetInstance->GetPosition();
			auto to_target = target - current_pos;
			auto distance_to_target = to_target.Magnitude();
			if (distance_to_target <= 0.4f) {
				SetVelocity(3 * to_target);
				SetColor(1.0f, 0, 0, 1.0f);

				hunger_time -= targetInstance->GetValue() / 100 * hunger_time;
				hunger_time = std::max(0.0f, hunger_time);

				handler.QueueRemoveEntity(target_id);
				return;
			}
		}

		targetInstance = spatial_handler.FindNearest<FlockingEntity>(current_pos);
		if (!targetInstance) {
			return;
		}

		target_id = targetInstance->GetId();
		auto    target = targetInstance->GetPosition();
		auto    to_target = target - current_pos;
		Vector3 direction = to_target.Normalized();

		Vector3 spread = Vector3(0, 0, 0);
		auto    avoids = spatial_handler.GetEntitiesInRadius<VectorDemoEntity>(current_pos, 1.0f);
		for (auto& a : avoids) {
			if (a.get() == this)
				continue;
			spread += (current_pos - a->GetPosition()).Normalized();
		}

		// Add some orbital motion using cross product
		Vector3 up = Vector3::Up();
		Vector3 tangent = direction.Cross(up).Normalized();

		// Combine linear movement with orbital motion
		Vector3 linear_vel = direction * 2.0f;
		Vector3 orbital_vel = tangent * sin(phase_ * 3.0f) * 1.5f;
		Vector3 total_velocity = linear_vel + orbital_vel + spread;

		SetVelocity(total_velocity);

		hunger_time += delta_time;
		hunger_time = std::min(100.0f, hunger_time);
		if (hunger_time < 5) {
			energy += delta_time;
		} else if (hunger_time > 15) {
			energy -= delta_time;
		}

		if (energy < 10) {
			logger::LOG("DEAD Preadator");

			handler.QueueRemoveEntity(GetId());
		} else if (energy >= 60) {
			logger::LOG("New Preadator");
			energy -= 25;
			handler.QueueAddEntity<VectorDemoEntity>(GetPosition());
		}

		// Color based on velocity magnitude and direction
		Vector3 vel = GetVelocity();
		float   speed = vel.Magnitude();
		Vector3 vel_normalized = vel.Normalized();

		float r = 0.5f + 0.5f * std::abs(vel_normalized.x);
		float g = 0.5f + 0.5f * std::abs(vel_normalized.y);
		float b = 0.5f + 0.3f * (speed / 5.0f); // Blue based on speed
		SetColor(r, g, b, 1.0f);
		SetTrailLength(2 * energy);
	}

} // namespace Boidsish
