#pragma once

#include <map>
#include <mutex>
#include <random>
#include <set>

#include "spatial_entity_handler.h"

namespace Boidsish {

	class GuidedMissileLauncher;
	class Terrain; // Forward declaration
	extern int selected_weapon;

	class PaperPlaneHandler: public SpatialEntityHandler {
	public:
		PaperPlaneHandler(task_thread_pool::task_thread_pool& thread_pool);
		void PreTimestep(float time, float delta_time) override;

		void RecordTarget(std::shared_ptr<GuidedMissileLauncher> target) const;
		int  GetTargetCount(std::shared_ptr<GuidedMissileLauncher> target) const;

	private:
		mutable std::mutex                    target_mutex_;
		mutable std::map<int, int>            target_counts_;
		std::map<const Terrain*, int>         spawned_launchers_;
		std::random_device                    rd_;
		std::mt19937                          eng_;
		float                                 damage_timer_ = 0.0f;
		std::uniform_real_distribution<float> damage_dist_;
	};

} // namespace Boidsish
