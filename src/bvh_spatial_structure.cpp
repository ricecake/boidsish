#include "bvh_spatial_structure.h"
#include "entity.h"

#define TINYBVH_IMPLEMENTATION
#include <tiny_bvh.h>
#include <typeindex>
#include <algorithm>

namespace Boidsish {

    struct BvhSpatialStructure::Impl {
        tinybvh::BVH bvh;
        std::vector<tinybvh::bvhvec4> bvh_aabbs;
        std::vector<int> entity_ids;
        std::vector<glm::vec3> entity_positions;
        std::unordered_map<int, int> id_to_prim;

        void Rebuild(const std::vector<std::shared_ptr<EntityBase>>& entities) {
            id_to_prim.clear();
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
                auto pos = entity->GetPosition();
                float size = entity->GetSize() * 0.5f;

                bvh_aabbs[i * 2] = tinybvh::bvhvec4(pos.x - size, pos.y - size, pos.z - size, 0.0f);
                bvh_aabbs[i * 2 + 1] = tinybvh::bvhvec4(pos.x + size, pos.y + size, pos.z + size, 0.0f);
                entity_ids[i] = entity->GetId();
                entity_positions[i] = glm::vec3(pos.x, pos.y, pos.z);
                id_to_prim[entity_ids[i]] = (int)i;
            }

            bvh.BuildAABB(bvh_aabbs.data(), (uint32_t)entities.size());
        }

        static bool SphereAABBIntersect(
            const glm::vec3& center,
            float radius_sq,
            const tinybvh::bvhvec3& min,
            const tinybvh::bvhvec3& max
        ) {
            float dmin = 0;
            if (center.x < min.x) dmin += (min.x - center.x) * (min.x - center.x);
            else if (center.x > max.x) dmin += (center.x - max.x) * (center.x - max.x);

            if (center.y < min.y) dmin += (min.y - center.y) * (min.y - center.y);
            else if (center.y > max.y) dmin += (center.y - max.y) * (center.y - max.y);

            if (center.z < min.z) dmin += (min.z - center.z) * (min.z - center.z);
            else if (center.z > max.z) dmin += (center.z - max.z) * (center.z - max.z);

            return dmin <= radius_sq;
        }

        void RadiusSearchRecursive(
            uint32_t nodeIdx,
            const glm::vec3& center,
            float radius_sq,
            const std::vector<int>& allowed_prims,
            std::vector<int>& results
        ) const {
            const auto& node = bvh.bvhNode[nodeIdx];
            if (!SphereAABBIntersect(center, radius_sq, node.aabbMin, node.aabbMax))
                return;

            if (node.isLeaf()) {
                for (uint32_t i = 0; i < node.triCount; ++i) {
                    uint32_t primIdx = bvh.primIdx[node.leftFirst + i];

                    // Correctness: Must check distance to individual entity center!
                    glm::vec3 diff = entity_positions[primIdx] - center;
                    if (glm::dot(diff, diff) <= radius_sq) {
                        // Check if allowed
                        if (allowed_prims.empty() || std::binary_search(allowed_prims.begin(), allowed_prims.end(), (int)primIdx)) {
                            results.push_back(entity_ids[primIdx]);
                        }
                    }
                }
            } else {
                RadiusSearchRecursive(node.leftFirst, center, radius_sq, allowed_prims, results);
                RadiusSearchRecursive(node.leftFirst + 1, center, radius_sq, allowed_prims, results);
            }
        }

        void NearestNeighborRecursive(
            uint32_t nodeIdx,
            const glm::vec3& center,
            float& nearest_dist_sq,
            int& nearest_id,
            const std::vector<int>& allowed_prims
        ) const {
            const auto& node = bvh.bvhNode[nodeIdx];

            float dmin = 0;
            if (center.x < node.aabbMin.x) dmin += (node.aabbMin.x - center.x) * (node.aabbMin.x - center.x);
            else if (center.x > node.aabbMax.x) dmin += (center.x - node.aabbMax.x) * (center.x - node.aabbMax.x);
            if (center.y < node.aabbMin.y) dmin += (node.aabbMin.y - center.y) * (node.aabbMin.y - center.y);
            else if (center.y > node.aabbMax.y) dmin += (center.y - node.aabbMax.y) * (center.y - node.aabbMax.y);
            if (center.z < node.aabbMin.z) dmin += (node.aabbMin.z - center.z) * (node.aabbMin.z - center.z);
            else if (center.z > node.aabbMax.z) dmin += (center.z - node.aabbMax.z) * (center.z - node.aabbMax.z);

            if (dmin >= nearest_dist_sq) return;

            if (node.isLeaf()) {
                for (uint32_t i = 0; i < node.triCount; ++i) {
                    uint32_t primIdx = bvh.primIdx[node.leftFirst + i];

                    // Check if allowed
                    if (!allowed_prims.empty() && !std::binary_search(allowed_prims.begin(), allowed_prims.end(), (int)primIdx)) {
                        continue;
                    }

                    // Behavior: Use center-to-center distance as per original RTree usage
                    glm::vec3 diff = entity_positions[primIdx] - center;
                    float dist_sq = glm::dot(diff, diff);

                    if (dist_sq <= nearest_dist_sq) {
                        nearest_dist_sq = dist_sq;
                        nearest_id = entity_ids[primIdx];
                    }
                }
            } else {
                const auto& left = bvh.bvhNode[node.leftFirst];
                const auto& right = bvh.bvhNode[node.leftFirst + 1];

                auto get_node_dist_sq = [&](const tinybvh::BVH::BVHNode& n) {
                    float d = 0;
                    if (center.x < n.aabbMin.x) d += (n.aabbMin.x - center.x) * (n.aabbMin.x - center.x);
                    else if (center.x > n.aabbMax.x) d += (center.x - n.aabbMax.x) * (center.x - n.aabbMax.x);
                    if (center.y < n.aabbMin.y) d += (n.aabbMin.y - center.y) * (n.aabbMin.y - center.y);
                    else if (center.y > n.aabbMax.y) d += (center.y - n.aabbMax.y) * (center.y - n.aabbMax.y);
                    if (center.z < n.aabbMin.z) d += (n.aabbMin.z - center.z) * (n.aabbMin.z - center.z);
                    else if (center.z > n.aabbMax.z) d += (center.z - n.aabbMax.z) * (center.z - n.aabbMax.z);
                    return d;
                };

                float d_left = get_node_dist_sq(left);
                float d_right = get_node_dist_sq(right);

                if (d_left < d_right) {
                    NearestNeighborRecursive(node.leftFirst, center, nearest_dist_sq, nearest_id, allowed_prims);
                    NearestNeighborRecursive(node.leftFirst + 1, center, nearest_dist_sq, nearest_id, allowed_prims);
                } else {
                    NearestNeighborRecursive(node.leftFirst + 1, center, nearest_dist_sq, nearest_id, allowed_prims);
                    NearestNeighborRecursive(node.leftFirst, center, nearest_dist_sq, nearest_id, allowed_prims);
                }
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

    void BvhSpatialStructure::Rebuild(const std::vector<std::shared_ptr<EntityBase>>& entities) {
        impl_->Rebuild(entities);
    }

    std::vector<int> BvhSpatialStructure::GetEntityIdsInRadius(const glm::vec3& center, float radius, const std::vector<int>& allowed_ids) const {
        std::vector<int> results;
        if (impl_->entity_ids.empty()) return results;

        std::vector<int> allowed_prims;
        if (!allowed_ids.empty()) {
            for (int id : allowed_ids) {
                auto it = impl_->id_to_prim.find(id);
                if (it != impl_->id_to_prim.end()) {
                    allowed_prims.push_back(it->second);
                }
            }
            std::sort(allowed_prims.begin(), allowed_prims.end());
        }

        impl_->RadiusSearchRecursive(0, center, radius * radius, allowed_prims, results);
        return results;
    }

    int BvhSpatialStructure::FindNearestId(const glm::vec3& center, float max_radius, const std::vector<int>& allowed_ids) const {
        if (impl_->entity_ids.empty()) return -1;

        std::vector<int> allowed_prims;
        if (!allowed_ids.empty()) {
            for (int id : allowed_ids) {
                auto it = impl_->id_to_prim.find(id);
                if (it != impl_->id_to_prim.end()) {
                    allowed_prims.push_back(it->second);
                }
            }
            std::sort(allowed_prims.begin(), allowed_prims.end());
        }

        float nearest_dist_sq = max_radius * max_radius;
        int nearest_id = -1;
        impl_->NearestNeighborRecursive(0, center, nearest_dist_sq, nearest_id, allowed_prims);
        return nearest_id;
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
