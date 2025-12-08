#pragma once

#include <algorithm>
#include <limits>

#include "boidsish.h"
#include <RTree.h>

namespace Boidsish {

	class SpatialEntityHandler: public EntityHandler {
	public:
		SpatialEntityHandler() = default;

		using EntityHandler::AddEntity;

		template <typename T>
		std::vector<std::shared_ptr<T>> GetEntitiesInRadius(const Vector3& center, float radius) {
			std::vector<std::shared_ptr<T>> result;
			float                           min[] = {center.x - radius, center.y - radius, center.z - radius};
			float                           max[] = {center.x + radius, center.y + radius, center.z + radius};

			rtree_.Search(min, max, [&](int id) {
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
		) {
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
		void PostTimestep(float time, float delta_time) override {
			(void)time;
			(void)delta_time;
			rtree_.RemoveAll();
			for (const auto& pair : GetAllEntities()) {
				auto&         entity = pair.second;
				const Vector3 pos = entity->GetPosition();
				float         min[3] = {pos.x, pos.y, pos.z};
				float         max[3] = {pos.x, pos.y, pos.z};
				rtree_.Insert(min, max, entity->GetId());
			}
		}

	private:
		RTree<int, float, 3> rtree_;
	};

} // namespace Boidsish
