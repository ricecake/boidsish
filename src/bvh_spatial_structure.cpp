#include "bvh_spatial_structure.h"
#include "entity.h"

#define TINYBVH_IMPLEMENTATION
#include <tiny_bvh.h>
#include <typeindex>
#include <algorithm>
#include <optional>
#include <utility>
#include <mutex>
#include <atomic>

#include <nigh/lp_space.hpp>
#include <nigh/kdtree_batch.hpp>
#include <task_thread_pool.hpp>
#include <poolstl/poolstl.hpp>

namespace Boidsish {

    namespace {
        // Value type for Nigh: store shared_ptr directly for O(1) retrieval
        struct NighEntity {
            std::shared_ptr<EntityBase> entity;
            Eigen::Matrix<float, 3, 1> pos;
        };

        struct NighEntityKey {
            const Eigen::Matrix<float, 3, 1>& operator()(const NighEntity& e) const {
                return e.pos;
            }
        };

        using NighSpace = unc::robotics::nigh::metric::L2Space<float, 3>;
        // Use Concurrent for thread-safe, parallel insertions
        using NighTree = unc::robotics::nigh::Nigh<
            NighEntity,
            NighSpace,
            NighEntityKey,
            unc::robotics::nigh::Concurrent,
            unc::robotics::nigh::KDTreeBatch<64>>;
    }

    struct BvhSpatialStructure::Impl {
        mutable tinybvh::BVH bvh;
        mutable std::vector<tinybvh::bvhvec4> bvh_aabbs;
        std::vector<int> entity_ids;
        mutable std::atomic<bool> bvh_dirty{true};
        mutable std::mutex bvh_build_mutex;

        NighTree nigh_tree;

        void Update(const std::vector<std::shared_ptr<EntityBase>>& entities, ::task_thread_pool::task_thread_pool& pool) {
            // Nigh is chose for performant update capabilities, so we clear and re-insert in parallel.
            // KDTreeBatch insertion is very fast.
            nigh_tree.clear();

            if (entities.empty()) {
                entity_ids.clear();
                bvh_aabbs.clear();
                bvh_dirty.store(true, std::memory_order_release);
                return;
            }

            // 1. Parallel insertion into Nigh
            std::for_each(poolstl::par.on(pool), entities.begin(), entities.end(), [&](const auto& entity) {
                auto pos = entity->GetPosition();
                nigh_tree.insert({entity, {pos.x, pos.y, pos.z}});
            });

            // 2. Prepare data for lazy BVH building
            entity_ids.assign(entities.size(), -1);
            bvh_aabbs.assign(entities.size() * 2, tinybvh::bvhvec4(0.0f));

            for (size_t i = 0; i < entities.size(); ++i) {
                const auto& entity = entities[i];
                auto pos = entity->GetPosition();
                float size = entity->GetSize() * 0.5f;

                bvh_aabbs[i * 2] = tinybvh::bvhvec4(pos.x - size, pos.y - size, pos.z - size, 0.0f);
                bvh_aabbs[i * 2 + 1] = tinybvh::bvhvec4(pos.x + size, pos.y + size, pos.z + size, 0.0f);
                entity_ids[i] = entity->GetId();
            }

            bvh_dirty.store(true, std::memory_order_release);
        }

        void EnsureBvhBuilt() const {
            if (!bvh_dirty.load(std::memory_order_acquire)) return;
            std::lock_guard<std::mutex> lock(bvh_build_mutex);
            if (!bvh_dirty.load(std::memory_order_relaxed)) return;

            if (!entity_ids.empty()) {
                bvh.BuildAABB(bvh_aabbs.data(), (uint32_t)entity_ids.size());
            }
            bvh_dirty.store(false, std::memory_order_release);
        }

        void RaycastRecursive(
            uint32_t nodeIdx,
            tinybvh::Ray& ray,
            int& nearest_id
        ) const {
            const auto& node = bvh.bvhNode[nodeIdx];
            float t_node = tinybvh_intersect_aabb(ray, node.aabbMin, node.aabbMax);
            if (t_node >= ray.hit.t) return;

            if (node.isLeaf()) {
                for (uint32_t i = 0; i < node.triCount; ++i) {
                    uint32_t primIdx = bvh.primIdx[node.leftFirst + i];
                    const auto& p_min = bvh.fragment[primIdx].bmin;
                    const auto& p_max = bvh.fragment[primIdx].bmax;
                    float t_prim = tinybvh_intersect_aabb(ray, p_min, p_max);
                    if (t_prim < ray.hit.t) {
                        ray.hit.t = t_prim;
                        nearest_id = entity_ids[primIdx];
                    }
                }
            } else {
                const auto& left = bvh.bvhNode[node.leftFirst];
                const auto& right = bvh.bvhNode[node.leftFirst + 1];

                tinybvh::Ray ray_copy = ray;
                float t_left = tinybvh_intersect_aabb(ray_copy, left.aabbMin, left.aabbMax);
                float t_right = tinybvh_intersect_aabb(ray_copy, right.aabbMin, right.aabbMax);

                if (t_left < t_right) {
                    if (t_left < ray.hit.t) RaycastRecursive(node.leftFirst, ray, nearest_id);
                    if (t_right < ray.hit.t) RaycastRecursive(node.leftFirst + 1, ray, nearest_id);
                } else {
                    if (t_right < ray.hit.t) RaycastRecursive(node.leftFirst + 1, ray, nearest_id);
                    if (t_left < ray.hit.t) RaycastRecursive(node.leftFirst, ray, nearest_id);
                }
            }
        }
    };

    BvhSpatialStructure::BvhSpatialStructure() : impl_(std::make_unique<Impl>()) {}
    BvhSpatialStructure::~BvhSpatialStructure() = default;

    BvhSpatialStructure::BvhSpatialStructure(BvhSpatialStructure&&) noexcept = default;
    BvhSpatialStructure& BvhSpatialStructure::operator=(BvhSpatialStructure&&) noexcept = default;

    void BvhSpatialStructure::Update(const std::vector<std::shared_ptr<EntityBase>>& entities, ::task_thread_pool::task_thread_pool& pool) {
        impl_->Update(entities, pool);
    }

    std::vector<std::shared_ptr<EntityBase>> BvhSpatialStructure::GetEntitiesInRadius(const glm::vec3& center, float radius) const {
        std::vector<std::shared_ptr<EntityBase>> results;
        if (impl_->entity_ids.empty()) return results;

        Eigen::Matrix<float, 3, 1> q{center.x, center.y, center.z};
        std::vector<std::pair<NighEntity, float>> neighbors;

        // nigh uses max_size() as a flag for all neighbors within radius
        impl_->nigh_tree.nearest(neighbors, q, std::numeric_limits<std::size_t>::max(), radius);

        results.reserve(neighbors.size());
        for (const auto& pair : neighbors) {
            results.push_back(pair.first.entity);
        }

        return results;
    }

    std::shared_ptr<EntityBase> BvhSpatialStructure::FindNearest(const glm::vec3& center, float max_radius) const {
        if (impl_->entity_ids.empty()) return nullptr;

        Eigen::Matrix<float, 3, 1> q{center.x, center.y, center.z};
        std::vector<std::pair<NighEntity, float>> neighbors;
        impl_->nigh_tree.nearest(neighbors, q, 1, max_radius);

        if (!neighbors.empty()) {
            return neighbors[0].first.entity;
        }

        return nullptr;
    }

    bool BvhSpatialStructure::Raycast(const Ray& ray, float& out_t, int& out_entity_id) const {
        if (impl_->entity_ids.empty()) return false;

        impl_->EnsureBvhBuilt();

        tinybvh::Ray bvh_ray(
            tinybvh::bvhvec3(ray.origin.x, ray.origin.y, ray.origin.z),
            tinybvh::bvhvec3(ray.direction.x, ray.direction.y, ray.direction.z)
        );
        int nearest_id = -1;
        impl_->RaycastRecursive(0, bvh_ray, nearest_id);
        if (nearest_id != -1) {
            out_t = bvh_ray.hit.t;
            out_entity_id = nearest_id;
            return true;
        }
        return false;
    }

    bool BvhSpatialStructure::IsEmpty() const {
        return impl_->entity_ids.empty();
    }

} // namespace Boidsish
