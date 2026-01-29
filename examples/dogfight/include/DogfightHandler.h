#pragma once

#include "DogfightPlane.h"
#include "spatial_entity_handler.h"

namespace Boidsish {

	class DogfightHandler: public SpatialEntityHandler {
	public:
		DogfightHandler(
			task_thread_pool::task_thread_pool& thread_pool,
			std::shared_ptr<Visualizer>         visualizer = nullptr
		);

	protected:
		void PreTimestep(float time, float delta_time) override;

	private:
		void SpawnPlane(Team team);
	};

} // namespace Boidsish
