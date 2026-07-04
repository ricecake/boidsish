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
			std::vector<std::shared_ptr<T>> result;
			glm::vec3                       c(center.x, center.y, center.z);

			std::shared_lock lock(bvh_mutex_);
			auto             entities = bvh_.GetEntitiesInRadius(c, radius);

			for (auto& entity : entities) {
				if constexpr (std::is_same_v<T, EntityBase>) {
					result.push_back(std::static_pointer_cast<T>(entity));
				} else {
					auto typed_entity = std::dynamic_pointer_cast<T>(entity);
					if (typed_entity) {
						result.push_back(typed_entity);
					}
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

			glm::vec3 c(center.x, center.y, center.z);

			std::shared_lock lock(bvh_mutex_);

			if constexpr (std::is_same_v<T, EntityBase>) {
				auto entity = bvh_.FindNearest(c, 1e10f);
				return std::static_pointer_cast<T>(entity);
			} else {
				// For typed nearest, we search in increasing radii if needed,
				// or just use k-nearest from Nigh.
				// For now, let's fetch a batch and filter.
				// Since Nigh is fast, we can fetch more if the first one doesn't match.
				auto entity = bvh_.FindNearest(c, 1e10f);
				if (auto typed = std::dynamic_pointer_cast<T>(entity)) {
					return typed;
				}

				// Fallback to radius search if the absolute nearest is not of the requested type.
				// In many cases in this engine, the absolute nearest IS the target or there are few entities.
				auto near_entities = bvh_.GetEntitiesInRadius(c, 1e10f);
				for (auto& e : near_entities) {
					if (auto typed = std::dynamic_pointer_cast<T>(e)) {
						return typed;
					}
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
