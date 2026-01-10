#pragma once

#include <random>

#include "spatial_entity_handler.h"

namespace Boidsish {

	// Handler for vector demonstration
	class VectorDemoHandler: public SpatialEntityHandler {
		std::random_device rd;
		std::mt19937       eng;

	public:
		VectorDemoHandler(task_thread_pool::task_thread_pool& thread_pool);
		void PreTimestep(float time, float delta_time) override;
	};

} // namespace Boidsish
