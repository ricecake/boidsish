#pragma once

#include <algorithm>
#include <array>
#include <functional>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace Boidsish {

	/**
	 * @brief A high-performance, thread-safe (for reads) Octree implementation.
	 *
	 * @tparam T The data type stored in the octree.
	 */
	template <typename T>
	class Octree {
	public:
		struct AABB {
			glm::vec3 min;
			glm::vec3 max;

			bool contains(const glm::vec3& p) const {
				return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y && p.z >= min.z && p.z <= max.z;
			}

			bool overlaps(const AABB& other) const {
				return max.x >= other.min.x && min.x <= other.max.x && max.y >= other.min.y && min.y <= other.max.y &&
					max.z >= other.min.z && min.z <= other.max.z;
			}

			glm::vec3 center() const { return (min + max) * 0.5f; }

			glm::vec3 size() const { return max - min; }
		};

		struct Ray {
			glm::vec3 origin;
			glm::vec3 direction;
			glm::vec3 inv_direction;

			Ray(const glm::vec3& o, const glm::vec3& d): origin(o), direction(glm::normalize(d)) {
				inv_direction = 1.0f / direction;
			}

			bool intersectsAABB(const AABB& aabb, float& t_min, float& t_max) const {
				float t0 = 0.0f;
				float t1 = 1e18f; // Large value for infinity

				for (int i = 0; i < 3; ++i) {
					float inv_d = inv_direction[i];
					float t_near = (aabb.min[i] - origin[i]) * inv_d;
					float t_far = (aabb.max[i] - origin[i]) * inv_d;

					if (t_near > t_far)
						std::swap(t_near, t_far);

					t0 = t_near > t0 ? t_near : t0;
					t1 = t_far < t1 ? t_far : t1;

					if (t0 > t1)
						return false;
				}

				t_min = t0;
				t_max = t1;
				return true;
			}
		};

		struct Item {
			AABB bounds;
			T    data;
		};

		Octree(const AABB& boundary, int max_items = 8, int max_depth = 8):
			boundary_(boundary), max_items_(max_items), max_depth_(max_depth), depth_(0) {}

		void insert(const Item& item) {
			if (!boundary_.overlaps(item.bounds))
				return;
			insert_internal(item);
		}

		/**
		 * @brief Query items that overlap with the given AABB.
		 * Thread-safe for reads.
		 */
		void query(const AABB& range, std::vector<T>& found) const {
			if (!boundary_.overlaps(range))
				return;

			for (const auto& item : items_) {
				if (range.overlaps(item.bounds)) {
					found.push_back(item.data);
				}
			}

			if (!is_leaf()) {
				for (const auto& child : children_) {
					child->query(range, found);
				}
			}
		}

		/**
		 * @brief Query items that might intersect with the given ray.
		 * Returns items in nodes intersected by the ray.
		 * Thread-safe for reads.
		 */
		void raycast(const Ray& ray, std::vector<T>& found) const {
			float t_min, t_max;
			if (!ray.intersectsAABB(boundary_, t_min, t_max))
				return;

			for (const auto& item : items_) {
				float it_min, it_max;
				if (ray.intersectsAABB(item.bounds, it_min, it_max)) {
					found.push_back(item.data);
				}
			}

			if (!is_leaf()) {
				// We could sort children by distance here for better early exit in ray casting,
				// but since we return ALL potential items, order doesn't strictly matter for gathering.
				for (const auto& child : children_) {
					child->raycast(ray, found);
				}
			}
		}

		void clear() {
			items_.clear();
			for (auto& child : children_) {
				child.reset();
			}
		}

		bool is_leaf() const { return children_[0] == nullptr; }

		Octree(const AABB& boundary, int max_items, int max_depth, int depth):
			boundary_(boundary), max_items_(max_items), max_depth_(max_depth), depth_(depth) {}

	private:
		void insert_internal(const Item& item) {
			if (is_leaf()) {
				if (items_.size() < (size_t)max_items_ || depth_ >= max_depth_) {
					items_.push_back(item);
				} else {
					subdivide();
					// After subdivision, try to push existing items to children
					std::vector<Item> old_items = std::move(items_);
					items_.clear();
					for (const auto& old_item : old_items) {
						insert_to_children(old_item);
					}
					insert_to_children(item);
				}
			} else {
				insert_to_children(item);
			}
		}

		void insert_to_children(const Item& item) {
			bool fits_in_child = false;
			for (auto& child : children_) {
				// If the item fully fits in a child, put it there
				if (child->boundary_.contains(item.bounds.min) && child->boundary_.contains(item.bounds.max)) {
					child->insert_internal(item);
					fits_in_child = true;
					break;
				}
			}
			// If it doesn't fit fully in any child (overlaps multiple), keep it in this node
			if (!fits_in_child) {
				items_.push_back(item);
			}
		}

		void subdivide() {
			glm::vec3 center = boundary_.center();
			glm::vec3 half = boundary_.size() * 0.5f;
			glm::vec3 quarter = half * 0.5f;

			for (int i = 0; i < 8; ++i) {
				glm::vec3 new_center = center;
				new_center.x += quarter.x * (i & 4 ? 1.0f : -1.0f);
				new_center.y += quarter.y * (i & 2 ? 1.0f : -1.0f);
				new_center.z += quarter.z * (i & 1 ? 1.0f : -1.0f);

				AABB child_boundary = {new_center - quarter, new_center + quarter};
				children_[i] =
					std::make_unique<Octree>(child_boundary, max_items_, max_depth_, depth_ + 1);
			}
		}

		AABB                            boundary_;
		int                             max_items_;
		int                             max_depth_;
		int                             depth_;
		std::vector<Item>               items_;
		std::array<std::unique_ptr<Octree>, 8> children_;
	};

} // namespace Boidsish
