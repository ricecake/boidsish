#pragma once

#include "dot.h"
#include "entity.h"

namespace Boidsish {

	class MagentaBall: public Entity<Dot> {
	public:
		MagentaBall(int id, Vector3 pos, Vector3 vel);

		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;

	private:
		float lived_ = 0.0f;
		float lifetime_ = 10.0f;
		bool  has_cleared_ground_ = false;
	};

} // namespace Boidsish
