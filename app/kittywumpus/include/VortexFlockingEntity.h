#pragma once

#include "entity.h"

namespace Boidsish {

	class VortexFlockingEntity: public Entity<> {
	public:
		VortexFlockingEntity(int id);

		using EntityBase::OnHit;
		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;
		void OnHit(const EntityHandler& handler, float damage, const glm::vec3& hit_point) override;

		bool IsTargetable() const override { return health_ > 0 && !is_dying_; }

	private:
		float health_ = 10.0f;
		bool  is_dying_ = false;
		float dissolve_timer_ = 0.0f;
	};

} // namespace Boidsish
