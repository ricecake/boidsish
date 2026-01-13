#pragma once

#include <random>

#include "spatial_entity_handler.h"

namespace Boidsish {
    class Visualizer;

	// Handler for vector demonstration
	class VectorDemoHandler: public SpatialEntityHandler {
		std::random_device rd;
		std::mt19937       eng;
        Visualizer& viz_;

	public:
		using SpatialEntityHandler::AddEntity;

		VectorDemoHandler(task_thread_pool::task_thread_pool& thread_pool, Visualizer& viz);
		void PreTimestep(float time, float delta_time) override;

        void AddEntity(int id, std::shared_ptr<EntityBase> entity) override;
        void RemoveEntity(int id) override;
	};

} // namespace Boidsish
