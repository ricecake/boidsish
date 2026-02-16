#pragma once

#include <memory>
#include <vector>

#include "entity.h"

namespace Boidsish {

	// Forward declarations
	class VectorDemoEntity;

	class FlockingEntity: public Entity<> {
	public:
		FlockingEntity(const Vector3& start_pos);
		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;

		float GetValue() const { return energy; }

	private:
		float hunger_time = 0.0f;
		float energy = 50.0f;

		Vector3 CalculateSeparation(
			const std::vector<std::shared_ptr<FlockingEntity>>&   neighbors,
			const std::vector<std::shared_ptr<VectorDemoEntity>>& predators
		);
		Vector3 CalculateAlignment(const std::vector<std::shared_ptr<FlockingEntity>>& neighbors);
		Vector3 CalculateCohesion(const std::vector<std::shared_ptr<FlockingEntity>>& neighbors);
	};

} // namespace Boidsish
