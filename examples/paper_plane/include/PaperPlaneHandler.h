#pragma once

#include <map>
#include <random>
#include <set>
#include <vector>
#include <memory>

#include "spatial_entity_handler.h"
#include "shape.h"

namespace Boidsish {

	class Terrain; // Forward declaration
	extern int selected_weapon;

	class PaperPlaneHandler: public SpatialEntityHandler {
	public:
        using SpatialEntityHandler::AddEntity;
		PaperPlaneHandler(task_thread_pool::task_thread_pool& thread_pool);
		void PreTimestep(float time, float delta_time) override;
        void PostTimestep(float time, float delta_time) override;
        std::vector<std::shared_ptr<Shape>> operator()(float time);
        void AddEntity(int id, std::shared_ptr<EntityBase> entity) override;

	private:
		std::map<const Terrain*, int>         spawned_launchers_;
		std::random_device                    rd_;
		std::mt19937                          eng_;
		float                                 damage_timer_ = 0.0f;
		std::uniform_real_distribution<float> damage_dist_;
	};

} // namespace Boidsish
