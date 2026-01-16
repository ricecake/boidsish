#pragma once

#include "entity.h"

// Forward declaration to avoid including the full thread pool header.
namespace task_thread_pool {
	class task_thread_pool;
}

namespace Boidsish {

	class VortexFlockingHandler: public EntityHandler {
	public:
		VortexFlockingHandler(task_thread_pool::task_thread_pool& thread_pool, std::shared_ptr<Visualizer>& visualizer);
	};

} // namespace Boidsish
