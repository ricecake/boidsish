#pragma once

#include <algorithm>
#include <limits>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <typeindex>
#include <vector>

#include "entity.h"
#include "graphics.h"
#include "bvh_spatial_structure.h"

namespace Boidsish {

	class SpatialEntityHandler: public EntityHandler {
	public:
		SpatialEntityHandler(
			task_thread_pool::task_thread_pool& thread_pool,
			std::shared_ptr<Visualizer>         visualizer = nullptr
		);

		virtual ~SpatialEntityHandler();

		using EntityHandler::AddEntity;

		template <typename T>
		std::vector<std::shared_ptr<T>> GetEntitiesInRadius(const Vector3& center, float radius) const {
			std::vector<int> allowed_ids;
			if constexpr (!std::is_same_v<T, EntityBase>) {
				auto typed_entities = GetEntitiesByType<T>();
				for (auto* e : typed_entities) {
					allowed_ids.push_back(e->GetId());
				}
			}

			std::vector<std::shared_ptr<T>> result;
			glm::vec3                       c(center.x, center.y, center.z);

			std::shared_lock lock(bvh_mutex_);
			auto             ids = bvh_.GetEntityIdsInRadius(c, radius, allowed_ids);

			for (int id : ids) {
				auto entity = GetEntity(id);
				if (entity) {
					result.push_back(std::static_pointer_cast<T>(entity));
				}
			}

			return result;
		}

		template <typename T>
		std::shared_ptr<T> FindNearest(
			const Vector3& center,
			float          initial_radius = 1.0f,
			float          expansion_factor = 2.0f,
			int            max_expansions = 10
		) const {
			(void)initial_radius;
			(void)expansion_factor;
			(void)max_expansions;

			std::vector<int> allowed_ids;
			if constexpr (!std::is_same_v<T, EntityBase>) {
				auto typed_entities = GetEntitiesByType<T>();
				for (auto* e : typed_entities) {
					allowed_ids.push_back(e->GetId());
				}
			}

			glm::vec3        c(center.x, center.y, center.z);
			std::shared_lock lock(bvh_mutex_);
			int              id = bvh_.FindNearestId(c, 1e10f, allowed_ids);
			if (id != -1) {
				auto entity = GetEntity(id);
				if (entity) {
					return std::static_pointer_cast<T>(entity);
				}
			}
			return nullptr;
		}

		/**
		 * @brief BVH-accelerated raycasting against all entities.
		 */
		std::shared_ptr<EntityBase>
		RaycastEntities(const Ray& ray, float& out_t, glm::vec3& out_hit_point) const override;

	protected:
		// BVH is rebuilt from scratch every frame, so we don't need incremental updates.
		void OnEntityUpdated(std::shared_ptr<EntityBase> entity) override { (void)entity; }

		void PostTimestep(float time, float delta_time) override;

	private:
		BvhSpatialStructure bvh_;
		BvhSpatialStructure next_bvh_; // Double buffering
		mutable std::shared_mutex bvh_mutex_;
	};

} // namespace Boidsish
