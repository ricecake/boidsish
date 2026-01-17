#pragma once

#include <algorithm>
#include <limits>
#include <memory>
#include <mutex>

#include "entity.h"
#include "graphics.h"
#include <RTree.h>

namespace Boidsish {

	class SpatialEntityHandler: public EntityHandler {
	public:
		SpatialEntityHandler(
			task_thread_pool::task_thread_pool& thread_pool,
			std::shared_ptr<Visualizer>         visualizer = nullptr
		):
			EntityHandler(thread_pool, visualizer),
			read_rtree_(std::make_unique<RTree<int, float, 3>>()),
			write_rtree_(std::make_unique<RTree<int, float, 3>>()) {}

		using EntityHandler::AddEntity;

		template <typename T>
		std::vector<std::shared_ptr<T>> GetEntitiesInRadius(const Vector3& center, float radius) const {
			std::vector<std::shared_ptr<T>> result;
			float                           min[] = {center.x - radius, center.y - radius, center.z - radius};
			float                           max[] = {center.x + radius, center.y + radius, center.z + radius};

			read_rtree_->Search(min, max, [&](int id) {
				auto entity = GetEntity(id);
				if (entity && entity->GetPosition().DistanceTo(center) <= radius) {
					auto typed_entity = std::dynamic_pointer_cast<T>(entity);
					if (typed_entity) {
						result.push_back(typed_entity);
					}
				}
				return true; // continue searching
			});

			return result;
		}

		template <typename T>
		std::shared_ptr<T> FindNearest(
			const Vector3& center,
			float          initial_radius = 1.0f,
			float          expansion_factor = 2.0f,
			int            max_expansions = 10
		) const {
			float radius = initial_radius;
			for (int i = 0; i < max_expansions; ++i) {
				auto entities_in_radius = GetEntitiesInRadius<T>(center, radius);
				if (!entities_in_radius.empty()) {
					return *std::min_element(
						entities_in_radius.begin(),
						entities_in_radius.end(),
						[&](const auto& a, const auto& b) {
							return center.DistanceTo(a->GetPosition()) < center.DistanceTo(b->GetPosition());
						}
					);
				}
				radius *= expansion_factor;
			}
			return nullptr;
		}

	protected:
		void OnEntityUpdated(std::shared_ptr<EntityBase> entity) override {
			const Vector3 pos = entity->GetPosition();
			float         min[3] = {pos.x, pos.y, pos.z};
			float         max[3] = {pos.x, pos.y, pos.z};
			std::lock_guard<std::mutex> lock(write_mutex_);
			write_rtree_->Insert(min, max, entity->GetId());
		}

		void PostTimestep(float time, float delta_time) override {
			(void)time;
			(void)delta_time;
			read_rtree_.swap(write_rtree_);
			write_rtree_->RemoveAll();
		}

	private:
		mutable std::unique_ptr<RTree<int, float, 3>> read_rtree_;
		mutable std::unique_ptr<RTree<int, float, 3>> write_rtree_;
		mutable std::mutex                            write_mutex_;
	};

} // namespace Boidsish
