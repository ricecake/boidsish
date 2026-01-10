#pragma once

#include "entity.h"

namespace Boidsish {

	class FruitEntity: public Entity<> {
	public:
		FruitEntity(int id);
		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;

		float GetValue() const { return value; }

	private:
		float phase_;
		float value;
	};

} // namespace Boidsish
