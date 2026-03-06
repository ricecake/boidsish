#pragma once

#include "entity.h"
#include "model.h"

namespace Boidsish {

	class Swooper: public Entity<Model> {
	public:
		Swooper(int id, Vector3 pos);

		using EntityBase::OnHit;
		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;
		void OnHit(const EntityHandler& handler, float damage, const glm::vec3& hit_point) override;

		bool IsTargetable() const override { return health_ > 0 && !is_dying_; }

	private:
		float health_ = 40.0f;
		bool  is_dying_ = false;
		float dissolve_timer_ = 0.0f;
		float zigzag_phase_ = 0.0f;
		float zigzag_speed_ = 3.0f;
		float zigzag_amplitude_ = 20.0f;
		float fire_cooldown_ = 1.0f;
		float time_to_fire_ = 0.0f;
		bool  swooping_ = false;
		float speed_ = 30.0f;
		bool  repositioning_ = false;
	};

} // namespace Boidsish
