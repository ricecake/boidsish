#pragma once

#include "entity.h"

namespace Boidsish {

	class VortexFlockingEntity: public Entity<> {
	public:
		VortexFlockingEntity();
		void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;
	};

} // namespace Boidsish
