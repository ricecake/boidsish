#include "bvh_spatial_structure.h"
#include "entity.h"

#define TINYBVH_IMPLEMENTATION
#include <tiny_bvh.h>
#include <typeindex>
#include <algorithm>
#include <optional>
#include <utility>

#include <nigh/lp_space.hpp>
#include <nigh/kdtree_batch.hpp>

namespace Boidsish {

    namespace {
        // Standard value type for Nigh to avoid complexity
        struct NighEntity {
            int id;
            Eigen::Matrix<float, 3, 1> pos;
        };

        struct NighEntityKey {
            const Eigen::Matrix<float, 3, 1>& operator()(const NighEntity& e) const {
                return e.pos;
            }
        };

        using NighSpace = unc::robotics::nigh::metric::L2Space<float, 3>;
        using NighTree = unc::robotics::nigh::Nigh<
            NighEntity,
            NighSpace,
            NighEntityKey,
            unc::robotics::nigh::NoThreadSafety,
            unc::robotics::nigh::KDTreeBatch<64>>;
    }

    struct BvhSpatialStructure::Impl {
        tinybvh::BVH bvh;
        std::vector<tinybvh::bvhvec4> bvh_aabbs;
        std::vector<int> entity_ids;
        std::vector<glm::vec3> entity_positions;
        std::unordered_map<int, int> id_to_prim;

        NighTree nigh_tree;

        void Update(const std::vector<std::shared_ptr<EntityBase>>& entities) {
            bool needs_rebuild = entities.size() != entity_ids.size();
            if (!needs_rebuild) {
                for (size_t i = 0; i < entities.size(); ++i) {
                    if (entities[i]->GetId() != entity_ids[i]) {
                        needs_rebuild = true;
                        break;
                    }
                }
            }

            if (needs_rebuild) {
                Rebuild(entities);
            } else {
                Refit(entities);
            }
        }

        void Rebuild(const std::vector<std::shared_ptr<EntityBase>>& entities) {
            id_to_prim.clear();
            nigh_tree.clear();
            if (entities.empty()) {
                bvh_aabbs.clear();
                entity_ids.clear();
                entity_positions.clear();
                return;
            }

            bvh_aabbs.assign(entities.size() * 2, tinybvh::bvhvec4(0.0f));
            entity_ids.assign(entities.size(), -1);
            entity_positions.assign(entities.size(), glm::vec3(0.0f));

            for (size_t i = 0; i < entities.size(); ++i) {
                const auto& entity = entities[i];
                auto        pos = entity->GetPosition();
                float       size = entity->GetSize() * 0.5f;

                bvh_aabbs[i * 2] = tinybvh::bvhvec4(pos.x - size, pos.y - size, pos.z - size, 0.0f);
                bvh_aabbs[i * 2 + 1] = tinybvh::bvhvec4(pos.x + size, pos.y + size, pos.z + size, 0.0f);
                entity_ids[i] = entity->GetId();
                entity_positions[i] = glm::vec3(pos.x, pos.y, pos.z);
                id_to_prim[entity_ids[i]] = (int)i;

                nigh_tree.insert({entity_ids[i], {pos.x, pos.y, pos.z}});
            }

            bvh.BuildAABB(bvh_aabbs.data(), (uint32_t)entities.size());
        }

        void Refit(const std::vector<std::shared_ptr<EntityBase>>& entities) {
            if (entities.empty())
                return;

            nigh_tree.clear();
            // Update AABBs and positions
            for (size_t i = 0; i < entities.size(); ++i) {
                const auto& entity = entities[i];
                auto        pos = entity->GetPosition();
                float       size = entity->GetSize() * 0.5f;

                bvh_aabbs[i * 2] = tinybvh::bvhvec4(pos.x - size, pos.y - size, pos.z - size, 0.0f);
                bvh_aabbs[i * 2 + 1] = tinybvh::bvhvec4(pos.x + size, pos.y + size, pos.z + size, 0.0f);
                entity_positions[i] = glm::vec3(pos.x, pos.y, pos.z);

                nigh_tree.insert({entity_ids[i], {pos.x, pos.y, pos.z}});
            }

            // Manually refit the BVH nodes using updated AABBs
            RefitRecursive(0);
            bvh.aabbMin = bvh.bvhNode[0].aabbMin;
            bvh.aabbMax = bvh.bvhNode[0].aabbMax;
        }

        void RefitRecursive(uint32_t nodeIdx) {
            auto& node = bvh.bvhNode[nodeIdx];
            if (node.isLeaf()) {
                tinybvh::bvhvec3 bmin(BVH_FAR), bmax(-BVH_FAR);
                for (uint32_t i = 0; i < node.triCount; ++i) {
                    uint32_t primIdx = bvh.primIdx[node.leftFirst + i];
                    bmin = tinybvh_min(bmin, tinybvh::bvhvec3(bvh_aabbs[primIdx * 2]));
                    bmax = tinybvh_max(bmax, tinybvh::bvhvec3(bvh_aabbs[primIdx * 2 + 1]));
                }
                node.aabbMin = bmin;
                node.aabbMax = bmax;
            } else {
                RefitRecursive(node.leftFirst);
                RefitRecursive(node.leftFirst + 1);
                const auto& left = bvh.bvhNode[node.leftFirst];
                const auto& right = bvh.bvhNode[node.leftFirst + 1];
                node.aabbMin = tinybvh_min(left.aabbMin, right.aabbMin);
                node.aabbMax = tinybvh_max(left.aabbMax, right.aabbMax);
            }
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

    void BvhSpatialStructure::Update(const std::vector<std::shared_ptr<EntityBase>>& entities) {
        impl_->Update(entities);
    }

    std::vector<int> BvhSpatialStructure::GetEntityIdsInRadius(const glm::vec3& center, float radius, const std::vector<int>& allowed_ids) const {
        std::vector<int> results;
        if (impl_->entity_ids.empty()) return results;

        Eigen::Matrix<float, 3, 1> q{center.x, center.y, center.z};
        std::vector<std::pair<NighEntity, float>> neighbors;

        // nigh uses max_size() as a flag for all neighbors within radius
        impl_->nigh_tree.nearest(neighbors, q, std::numeric_limits<std::size_t>::max(), radius);

        if (allowed_ids.empty()) {
            for (const auto& pair : neighbors) {
                results.push_back(pair.first.id);
            }
        } else {
            // Optimization: if allowed_ids is small, it might be better to check against it
            // if large, sorting it for binary search is better.
            std::vector<int> sorted_allowed = allowed_ids;
            std::sort(sorted_allowed.begin(), sorted_allowed.end());
            for (const auto& pair : neighbors) {
                if (std::binary_search(sorted_allowed.begin(), sorted_allowed.end(), pair.first.id)) {
                    results.push_back(pair.first.id);
                }
            }
        }

        return results;
    }

    int BvhSpatialStructure::FindNearestId(const glm::vec3& center, float max_radius, const std::vector<int>& allowed_ids) const {
        if (impl_->entity_ids.empty()) return -1;

        Eigen::Matrix<float, 3, 1> q{center.x, center.y, center.z};

        if (allowed_ids.empty()) {
            std::vector<std::pair<NighEntity, float>> neighbors;
            impl_->nigh_tree.nearest(neighbors, q, 1, max_radius);
            if (!neighbors.empty()) {
                return neighbors[0].first.id;
            }
        } else {
            // Nigh doesn't support filtering natively in the nearest() call easily without
            // potentially many re-queries if the nearest isn't allowed.
            // We can use the k-nearest version and find the first allowed one.
            std::vector<std::pair<NighEntity, float>> neighbors;
            // Fetch some reasonable number of neighbors, or all if needed.
            // Since we want the ABSOLUTE nearest allowed, and Nigh is fast,
            // we can fetch all in radius and pick the first one.
            impl_->nigh_tree.nearest(neighbors, q, std::numeric_limits<std::size_t>::max(), max_radius);

            std::vector<int> sorted_allowed = allowed_ids;
            std::sort(sorted_allowed.begin(), sorted_allowed.end());

            for (const auto& pair : neighbors) {
                if (std::binary_search(sorted_allowed.begin(), sorted_allowed.end(), pair.first.id)) {
                    return pair.first.id;
                }
            }
        }

        return -1;
    }

    bool BvhSpatialStructure::Raycast(const Ray& ray, float& out_t, int& out_entity_id) const {
        if (impl_->entity_ids.empty()) return false;
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
