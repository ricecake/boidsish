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
#include "spatial_structure.h"

namespace Boidsish {

	class SpatialEntityHandler: public EntityHandler {
	public:
		SpatialEntityHandler(
			task_thread_pool::task_thread_pool& thread_pool,
			std::shared_ptr<Visualizer>         visualizer = nullptr
		);

		virtual ~SpatialEntityHandler();

		void AddEntity(int id, std::shared_ptr<EntityBase> entity) override {
			EntityHandler::AddEntity(id, entity);
			spatial_structure_.BufferAdd(entity);
		}

		void RemoveEntity(int id) override {
			EntityHandler::RemoveEntity(id);
			spatial_structure_.BufferRemove(id);
		}

		using EntityHandler::AddEntity;

		template <typename T>
		std::vector<std::shared_ptr<T>> GetEntitiesInRadius(const Vector3& center, float radius) const {
			std::vector<std::shared_ptr<T>> result;
			glm::vec3                       c(center.x, center.y, center.z);

			spatial_structure_.QueryRadius(c, radius, [&](std::shared_ptr<EntityBase> entity) {
				if constexpr (std::is_same_v<T, EntityBase>) {
					result.push_back(std::static_pointer_cast<T>(entity));
				} else {
					auto typed_entity = std::dynamic_pointer_cast<T>(entity);
					if (typed_entity) {
						result.push_back(typed_entity);
					}
				}
			});

			return result;
		}

		template <typename T>
		std::shared_ptr<T> FindNearest(
			const Vector3& center,
			float          initial_radius = 1000.0f,
			float          expansion_factor = 2.0f,
			int            max_expansions = 10
		) const {
			(void)initial_radius;
			(void)expansion_factor;
			(void)max_expansions;

			glm::vec3 c(center.x, center.y, center.z);

			auto filter = [](std::shared_ptr<EntityBase> entity) -> bool {
				if constexpr (std::is_same_v<T, EntityBase>) {
					return true;
				} else {
					return dynamic_cast<T*>(entity.get()) != nullptr;
				}
			};

			auto entity = spatial_structure_.QueryNearest(c, initial_radius, filter);
			if (entity) {
				return std::static_pointer_cast<T>(entity);
			}
			return nullptr;
		}

		/**
		 * @brief BVH-accelerated raycasting against all entities.
		 */
		std::shared_ptr<EntityBase>
		RaycastEntities(const Ray& ray, float& out_t, glm::vec3& out_hit_point) const override;

	protected:
		void PreTimestep(float time, float delta_time) override;

		void OnEntityUpdated(std::shared_ptr<EntityBase> entity) override {
			spatial_structure_.BufferUpdate(entity);
		}

		void PostTimestep(float time, float delta_time) override;

	private:
		SpatialStructure spatial_structure_;
	};

} // namespace Boidsish
