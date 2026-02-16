#include "VortexFlockingHandler.h"

#include <random>

#include "VortexFlockingEntity.h"

namespace Boidsish {

	VortexFlockingHandler::VortexFlockingHandler(
		task_thread_pool::task_thread_pool& thread_pool,
		std::shared_ptr<Visualizer>&        visualizer
	):
		EntityHandler(thread_pool, visualizer) {
		std::random_device               rd;
		std::mt19937                     gen(rd());
		std::uniform_real_distribution<> dis_pos(-40.0, 40.0);
		std::uniform_real_distribution<> dis_y(30.0, 90.0);
		std::uniform_real_distribution<> dis_vel(-5.0, 5.0);

		for (int i = 0; i < 100; ++i) {
			auto entity = std::make_shared<VortexFlockingEntity>();
			entity->SetPosition(dis_pos(gen), dis_y(gen), dis_pos(gen));
			entity->SetVelocity(dis_vel(gen), dis_vel(gen), dis_vel(gen));
			AddEntity(entity);
		}
	}

} // namespace Boidsish
