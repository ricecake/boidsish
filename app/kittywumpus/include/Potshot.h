#pragma once

#include "entity.h"
#include "model.h"

namespace Boidsish {

	class Potshot: public Entity<Model> {
	public:
		Potshot(int id, Vector3 pos);

		using EntityBase::OnHit;
		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;
		void OnHit(const EntityHandler& handler, float damage, const glm::vec3& hit_point) override;

		bool IsTargetable() const override { return health_ > 0 && !is_dying_; }

	private:
		float     health_ = 30.0f;
		bool      is_dying_ = false;
		float     dissolve_timer_ = 0.0f;
		float     lifetime_ = 8.0f;
		float     lived_ = 0.0f;
		float     reposition_timer_ = 0.0f;
		glm::vec3 relative_target_pos_;
		int       shots_to_fire_ = 0;
		float     fire_timer_ = 0.0f;
		float     speed_ = 40.0f;
		bool      initialized_target_ = false;
		bool      repositioning_ = false;

		void PickNewPosition(const glm::vec3& player_forward);
	};

} // namespace Boidsish
