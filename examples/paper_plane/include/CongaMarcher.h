#pragma once

#include "entity.h"
#include "model.h"

namespace Boidsish {

	class CongaMarcher: public Entity<Model> {
	public:
		CongaMarcher(int id, Vector3 pos, int leader_id = -1);

		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;
		void OnHit(const EntityHandler& handler, float damage) override;

	private:
		int   leader_id_;
		float health_ = 20.0f;
		float spiral_phase_ = 0.0f;
		float spiral_speed_ = 3.0f;
		float spiral_radius_ = 8.0f;
		float speed_ = 45.0f;
	};

} // namespace Boidsish
