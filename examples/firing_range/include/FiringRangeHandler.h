#pragma once
#include "PaperPlaneHandler.h"

namespace Boidsish {
	enum class MissileType {
		Guided,
		Cat
	};

	class FiringRangeHandler : public PaperPlaneHandler {
	public:
		FiringRangeHandler(task_thread_pool::task_thread_pool& thread_pool);
		void PreTimestep(float time, float delta_time) override;

		bool auto_fire = false;
		MissileType auto_fire_type = MissileType::Guided;
		float fire_interval = 2.0f;
		float last_fire_time = 0.0f;
	};
}
