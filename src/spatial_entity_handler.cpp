#include "spatial_entity_handler.h"

namespace Boidsish {

	SpatialEntityHandler::SpatialEntityHandler(
		task_thread_pool::task_thread_pool& thread_pool,
		std::shared_ptr<Visualizer>         visualizer
	):
		EntityHandler(thread_pool, visualizer) {}

	SpatialEntityHandler::~SpatialEntityHandler() = default;

	void SpatialEntityHandler::PostTimestep(float time, float delta_time) {
		(void)time;
		(void)delta_time;

		// Collect all current entities
		std::vector<std::shared_ptr<EntityBase>> entities;
		auto all_entities = GetAllEntities();
		entities.reserve(all_entities.size());
		for (auto const& [id, entity] : all_entities) {
			entities.push_back(entity);
		}

		// Rebuild the "next" implementation (write buffer)
		next_bvh_.Update(entities);

		// Swap it into the active implementation (read buffer)
		std::unique_lock lock(bvh_mutex_);
		bvh_.swap(next_bvh_);
	}

	std::shared_ptr<EntityBase>
	SpatialEntityHandler::RaycastEntities(const Ray& ray, float& out_t, glm::vec3& out_hit_point) const {
		int id = -1;

		{
			std::shared_lock lock(bvh_mutex_);
			if (!bvh_.Raycast(ray, out_t, id)) {
				return nullptr;
			}
		}

		auto entity = GetEntity(id);
		if (entity) {
			out_hit_point = ray.origin + ray.direction * out_t;
			return entity;
		}

		return nullptr;
	}

} // namespace Boidsish
