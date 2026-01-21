#include "VortexFlockingEntity.h"

#include <cmath>
#include <iostream>

#include "spatial_entity_handler.h"
#include <glm/gtx/projection.hpp>

namespace Boidsish {

	VortexFlockingEntity::VortexFlockingEntity(int id): Entity(id) {
		SetSize(5.0f);
		SetTrailLength(30);
	}

	void VortexFlockingEntity::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		(void)time;       // Mark unused
		(void)delta_time; // Mark unused
		auto& spatial_handler = static_cast<const SpatialEntityHandler&>(handler);

		const auto& entities = spatial_handler.GetEntitiesInRadius<VortexFlockingEntity>(GetPosition(), 256.0f);
		if (entities.size() <= 1)
			return;

		// --- Parameters ---
		const Vector3 center_point(0, 70.0f, 0);
		const float   cohesion_factor = 0.05f;
		const float   separation_factor = 0.2f;
		const float   separation_distance = 10.0f;
		const float   vortex_strength = 0.8f;
		float         max_speed = 5 + (rand() % 10); // 10.0f;
		const float   terrain_avoidance_factor = 3.5f;
		const float   terrain_avoidance_height = 25.0f;
		bool          rightHandedNess = 1; //(rand() % 10) == 1;

		// --- Calculation ---
		Vector3 center_of_mass(0.0f, 0.0f, 0.0f);
		Vector3 separation_force(0.0f, 0.0f, 0.0f);
		int     neighbor_count = 0;

		for (const auto& entity : entities) {
			if (entity->GetId() == this->GetId())
				continue;

			center_of_mass += entity->GetPosition();
			neighbor_count++;

			float dist = GetPosition().DistanceTo(entity->GetPosition());
			if (dist < separation_distance) {
				Vector3 away_vec = GetPosition() - entity->GetPosition();
				separation_force += away_vec / (dist * dist); // Inverse square
			}
		}

		if (neighbor_count > 0) {
			center_of_mass /= static_cast<float>(neighbor_count);
		}

		Vector3 com_vec = (center_of_mass - GetPosition());
		// 1. Cohesion
		Vector3 cohesion_vec = com_vec * cohesion_factor;

		// 2. Separation
		Vector3 separation_vec = separation_force * separation_factor;

		// 3. Vortex Logic
		float   dist_to_com = GetPosition().DistanceTo(center_of_mass);
		Vector3 to_center_xz = Vector3(center_point.x - GetPosition().x, 0, center_point.z - GetPosition().z);
		to_center_xz.Normalize();

		// Circular motion (tangent to the circle around the center point)
		Vector3 circular_motion = Vector3(to_center_xz.z, 0, -to_center_xz.x);
		// Vector3 circ = Vector3(glm::proj(com_vec.Toglm(), circular_motion.Toglm()));
		// Vector3 circ = Vector3(glm::proj(circular_motion.Toglm(), com_vec.Toglm())); // Kinda neat

		// Spiral motion (inward and downward)
		// Vector3 spiral_motion = Vector3(to_center_xz.x, -0.4f, to_center_xz.z);
		Vector3 spiral_motion = circular_motion + com_vec + circular_motion.Cross(com_vec); // AWESOME
		// Vector3 spiral_motion = circular_motion + com_vec + (rightHandedNess? 1 : -1)
		// *circular_motion.Cross(com_vec); // AWESOME Vector3 spiral_motion = com_vec + ((rightHandedNess? 1 : -1) *
		// circular_motion.Cross(com_vec)); // AWESOME -- maybe not as much? Vector3 spiral_motion = circular_motion +
		// 2*com_vec + (rightHandedNess? 1 : -1) *circular_motion.Cross(com_vec); // AWESOME Vector3 spiral_motion =
		// circular_motion + circ + (rightHandedNess? 1 : -1) *circular_motion.Cross(com_vec); // AWESOME

		// Blend between circular and spiral based on distance from flock's center of mass
		float   blend_factor = std::min(1.0f, dist_to_com / 40.0f); // 80 is the effective "radius" of the flock
		Vector3 vortex_vec = (circular_motion * (1.0f - blend_factor) + spiral_motion * blend_factor) * vortex_strength;
		// Vector3 vortex_vec = (center_point * (1.0f - blend_factor) + spiral_motion * blend_factor) * vortex_strength;

		// --- Combine and Apply Forces ---
		Vector3 new_velocity = GetVelocity() + cohesion_vec + separation_vec + vortex_vec;

		// 4. Terrain Avoidance
		auto  terrain_props = handler.GetTerrainPointPropertiesThreadSafe(GetPosition().x, GetPosition().z);
		float height_above_terrain = GetPosition().y - std::get<0>(terrain_props);
		if (height_above_terrain < terrain_avoidance_height) {
			float avoidance_strength = (1.0f - (height_above_terrain / terrain_avoidance_height)) *
				terrain_avoidance_factor;
			new_velocity.y += avoidance_strength;
		}

		// --- Finalize ---
		// Limit speed
		if (new_velocity.MagnitudeSquared() > max_speed * max_speed) {
			new_velocity.Normalize();
			new_velocity *= max_speed;
		}

		SetVelocity(new_velocity);

		// Update color based on speed
		float speed = GetVelocity().Magnitude();
		float color_mix = std::min(1.0f, dist_to_com / 40.0f);
		SetColor(0.2f + color_mix * 0.8f, 1.0f - speed / max_speed, 0.8f, 1.0f);
	}

} // namespace Boidsish
