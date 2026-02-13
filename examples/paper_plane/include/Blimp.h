#pragma once

#include <random>

#include "entity.h"
#include "model.h"

namespace Boidsish {

	class Blimp: public Entity<Model> {
	public:
		Blimp(int id, Vector3 pos);

		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;
		void OnHit(const EntityHandler& handler, float damage) override;

		float GetHealth() const override { return health_; }

		bool IsTargetable() const override { return true; }

	private:
		float health_ = 500.0f;
		float max_health_ = 500.0f;
		float fire_timer_ = 0.0f;

		std::random_device rd_;
		std::mt19937       eng_;
	};

} // namespace Boidsish
