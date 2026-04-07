#pragma once

#include "entity.h"
#include "spatial_entity_handler.h"
#include "JetPlane.h"

namespace Boidsish {

	class JetHandler : public SpatialEntityHandler {
	public:
		JetHandler(task_thread_pool::task_thread_pool& thread_pool);

		void PreparePlane(std::shared_ptr<JetPlane> plane);
	};

} // namespace Boidsish
