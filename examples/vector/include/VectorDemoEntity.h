#pragma once

#include "CloneableDot.h"
#include "entity.h"

namespace Boidsish {

	// Forward declaration
	class FlockingEntity;

	class VectorDemoEntity: public Entity<> {
	public:
		VectorDemoEntity(int id, const Vector3& start_pos);
		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;

	private:
		float hunger_time = 0.0f;
		float energy = 50.0f;
		float phase_;
		int   target_id = -1;
	};

} // namespace Boidsish
