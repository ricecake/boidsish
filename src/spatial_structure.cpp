#include "spatial_structure.h"
#include "entity.h"
#include "constants.h"
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <vector>

#define TINYBVH_IMPLEMENTATION
#include <tiny_bvh.h>

namespace Boidsish {

    struct GridKey {
        int x, y, z;
        bool operator==(const GridKey& other) const {
            return x == other.x && y == other.y && z == other.z;
        }
    };

    struct GridKeyHash {
        size_t operator()(const GridKey& k) const {
            size_t h = std::hash<int>{}(k.x);
            h ^= std::hash<int>{}(k.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int>{}(k.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    struct SpatialStructure::Impl {
        float cell_size = 32.0f;
        std::unordered_map<GridKey, std::vector<std::shared_ptr<EntityBase>>, GridKeyHash> grid;
        std::vector<std::shared_ptr<EntityBase>> all_entities;

        Impl() {
            cell_size = (float)Constants::Class::Terrain::ChunkSize();
            if (cell_size <= 0.0f) cell_size = 32.0f;
        }

        GridKey GetKey(const glm::vec3& pos) const {
            return {
                (int)std::floor(pos.x / cell_size),
                (int)std::floor(pos.y / cell_size),
                (int)std::floor(pos.z / cell_size)
            };
        }

        void Update(const std::vector<std::shared_ptr<EntityBase>>& entities) {
            grid.clear();
            all_entities = entities;
            for (const auto& entity : entities) {
                glm::vec3 pos = entity->GetPosition().Toglm();
                float half_size = entity->GetSize() * 0.5f;
                glm::vec3 min_p = pos - glm::vec3(half_size);
                glm::vec3 max_p = pos + glm::vec3(half_size);

                GridKey min_k = GetKey(min_p);
                GridKey max_k = GetKey(max_p);

                for(int x = min_k.x; x <= max_k.x; ++x) {
                    for(int y = min_k.y; y <= max_k.y; ++y) {
                        for(int z = min_k.z; z <= max_k.z; ++z) {
                            grid[{x, y, z}].push_back(entity);
                        }
                    }
                }
            }
        }

        std::vector<int> GetEntityIdsInRadius(const glm::vec3& center, float radius, const std::vector<int>& allowed_ids) const {
            std::vector<int> results;
            float radius_sq = radius * radius;

            GridKey min_k = GetKey(center - glm::vec3(radius));
            GridKey max_k = GetKey(center + glm::vec3(radius));

            std::unordered_set<int> found_ids;

            for(int x = min_k.x; x <= max_k.x; ++x) {
                for(int y = min_k.y; y <= max_k.y; ++y) {
                    for(int z = min_k.z; z <= max_k.z; ++z) {
                        auto it = grid.find({x, y, z});
                        if (it != grid.end()) {
                            for (const auto& entity : it->second) {
                                int id = entity->GetId();
                                if (found_ids.count(id)) continue;

                                if (!allowed_ids.empty() && std::find(allowed_ids.begin(), allowed_ids.end(), id) == allowed_ids.end())
                                    continue;

                                glm::vec3 diff = entity->GetPosition().Toglm() - center;
                                if (glm::dot(diff, diff) <= radius_sq) {
                                    results.push_back(id);
                                    found_ids.insert(id);
                                }
                            }
                        }
                    }
                }
            }
            return results;
        }

        int FindNearestId(const glm::vec3& center, float max_radius, const std::vector<int>& allowed_ids) const {
            float nearest_dist_sq = max_radius * max_radius;
            int nearest_id = -1;

            // If the search radius is too large, it's more efficient to just iterate over all entities
            // instead of iterating over millions of empty grid cells.
            bool search_all = max_radius > cell_size * 8.0f; // Arbitrary threshold

            if (search_all) {
                for (const auto& entity : all_entities) {
                    int id = entity->GetId();
                    if (!allowed_ids.empty() && std::find(allowed_ids.begin(), allowed_ids.end(), id) == allowed_ids.end())
                        continue;

                    glm::vec3 diff = entity->GetPosition().Toglm() - center;
                    float dist_sq = glm::dot(diff, diff);
                    if (dist_sq < nearest_dist_sq) {
                        nearest_dist_sq = dist_sq;
                        nearest_id = id;
                    }
                }
                return nearest_id;
            }

            GridKey min_k = GetKey(center - glm::vec3(max_radius));
            GridKey max_k = GetKey(center + glm::vec3(max_radius));

            std::unordered_set<int> checked_ids;

            for(int x = min_k.x; x <= max_k.x; ++x) {
                for(int y = min_k.y; y <= max_k.y; ++y) {
                    for(int z = min_k.z; z <= max_k.z; ++z) {
                        auto it = grid.find({x, y, z});
                        if (it != grid.end()) {
                            for (const auto& entity : it->second) {
                                int id = entity->GetId();
                                if (checked_ids.count(id)) continue;
                                checked_ids.insert(id);

                                if (!allowed_ids.empty() && std::find(allowed_ids.begin(), allowed_ids.end(), id) == allowed_ids.end())
                                    continue;

                                glm::vec3 diff = entity->GetPosition().Toglm() - center;
                                float dist_sq = glm::dot(diff, diff);
                                if (dist_sq < nearest_dist_sq) {
                                    nearest_dist_sq = dist_sq;
                                    nearest_id = id;
                                }
                            }
                        }
                    }
                }
            }
            return nearest_id;
        }

        void RaycastRecursive(
            const tinybvh::BVH& bvh,
            uint32_t nodeIdx,
            tinybvh::Ray& ray,
            int& nearest_prim_idx
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
                        nearest_prim_idx = (int)primIdx;
                    }
                }
            } else {
                const auto& left = bvh.bvhNode[node.leftFirst];
                const auto& right = bvh.bvhNode[node.leftFirst + 1];

                tinybvh::Ray ray_copy = ray;
                float t_left = tinybvh_intersect_aabb(ray_copy, left.aabbMin, left.aabbMax);
                float t_right = tinybvh_intersect_aabb(ray_copy, right.aabbMin, right.aabbMax);

                if (t_left < t_right) {
                    if (t_left < ray.hit.t) RaycastRecursive(bvh, node.leftFirst, ray, nearest_prim_idx);
                    if (t_right < ray.hit.t) RaycastRecursive(bvh, node.leftFirst + 1, ray, nearest_prim_idx);
                } else {
                    if (t_right < ray.hit.t) RaycastRecursive(bvh, node.leftFirst + 1, ray, nearest_prim_idx);
                    if (t_left < ray.hit.t) RaycastRecursive(bvh, node.leftFirst, ray, nearest_prim_idx);
                }
            }
        }

        bool Raycast(const Ray& ray, float& out_t, int& out_entity_id) const {
            if (all_entities.empty()) return false;

            std::vector<std::shared_ptr<EntityBase>> candidates;
            std::unordered_set<int> candidate_ids;

            float max_ray_dist = 2000.0f;

            glm::vec3 pos = ray.origin;
            glm::vec3 dir = ray.direction;

            if (glm::length(dir) < 0.0001f) return false;

            glm::ivec3 cell = glm::ivec3(std::floor(pos.x / cell_size), std::floor(pos.y / cell_size), std::floor(pos.z / cell_size));
            glm::vec3 step = glm::sign(dir);

            glm::vec3 deltaT;
            deltaT.x = (dir.x != 0) ? std::abs(cell_size / dir.x) : 1e30f;
            deltaT.y = (dir.y != 0) ? std::abs(cell_size / dir.y) : 1e30f;
            deltaT.z = (dir.z != 0) ? std::abs(cell_size / dir.z) : 1e30f;

            glm::vec3 nextT;
            nextT.x = (dir.x > 0) ? (float(cell.x + 1) * cell_size - pos.x) / dir.x : (dir.x < 0 ? (float(cell.x) * cell_size - pos.x) / dir.x : 1e30f);
            nextT.y = (dir.y > 0) ? (float(cell.y + 1) * cell_size - pos.y) / dir.y : (dir.y < 0 ? (float(cell.y) * cell_size - pos.y) / dir.y : 1e30f);
            nextT.z = (dir.z > 0) ? (float(cell.z + 1) * cell_size - pos.z) / dir.z : (dir.z < 0 ? (float(cell.z) * cell_size - pos.z) / dir.z : 1e30f);

            float t = 0;
            // Limit search distance or until we exit some bounds if needed
            // For now use max_ray_dist
            while (t < max_ray_dist) {
                auto it = grid.find({cell.x, cell.y, cell.z});
                if (it != grid.end()) {
                    for (const auto& entity : it->second) {
                        int id = entity->GetId();
                        if (candidate_ids.find(id) == candidate_ids.end()) {
                            candidates.push_back(entity);
                            candidate_ids.insert(id);
                        }
                    }
                }

                if (nextT.x < nextT.y) {
                    if (nextT.x < nextT.z) {
                        t = nextT.x;
                        nextT.x += deltaT.x;
                        cell.x += (int)step.x;
                    } else {
                        t = nextT.z;
                        nextT.z += deltaT.z;
                        cell.z += (int)step.z;
                    }
                } else {
                    if (nextT.y < nextT.z) {
                        t = nextT.y;
                        nextT.y += deltaT.y;
                        cell.y += (int)step.y;
                    } else {
                        t = nextT.z;
                        nextT.z += deltaT.z;
                        cell.z += (int)step.z;
                    }
                }
            }

            if (candidates.empty()) return false;

            tinybvh::BVH bvh;
            std::vector<tinybvh::bvhvec4> bvh_aabbs(candidates.size() * 2);
            for (size_t i = 0; i < candidates.size(); ++i) {
                auto p = candidates[i]->GetPosition().Toglm();
                float s = candidates[i]->GetSize() * 0.5f;
                bvh_aabbs[i * 2] = tinybvh::bvhvec4(p.x - s, p.y - s, p.z - s, 0.0f);
                bvh_aabbs[i * 2 + 1] = tinybvh::bvhvec4(p.x + s, p.y + s, p.z + s, 0.0f);
            }
            bvh.BuildAABB(bvh_aabbs.data(), (uint32_t)candidates.size());

            tinybvh::Ray bvh_ray(
                tinybvh::bvhvec3(ray.origin.x, ray.origin.y, ray.origin.z),
                tinybvh::bvhvec3(ray.direction.x, ray.direction.y, ray.direction.z)
            );

            int nearest_prim_idx = -1;
            RaycastRecursive(bvh, 0, bvh_ray, nearest_prim_idx);

            if (nearest_prim_idx != -1) {
                out_t = bvh_ray.hit.t;
                out_entity_id = candidates[nearest_prim_idx]->GetId();
                return true;
            }

            return false;
        }
    };

    SpatialStructure::SpatialStructure() : impl_(std::make_unique<Impl>()) {}
    SpatialStructure::~SpatialStructure() = default;
    SpatialStructure::SpatialStructure(SpatialStructure&&) noexcept = default;
    SpatialStructure& SpatialStructure::operator=(SpatialStructure&&) noexcept = default;

    void SpatialStructure::swap(SpatialStructure& other) noexcept {
        std::swap(impl_, other.impl_);
    }

    void SpatialStructure::Update(const std::vector<std::shared_ptr<EntityBase>>& entities) {
        impl_->Update(entities);
    }

    std::vector<int> SpatialStructure::GetEntityIdsInRadius(const glm::vec3& center, float radius, const std::vector<int>& allowed_ids) const {
        return impl_->GetEntityIdsInRadius(center, radius, allowed_ids);
    }

    int SpatialStructure::FindNearestId(const glm::vec3& center, float max_radius, const std::vector<int>& allowed_ids) const {
        return impl_->FindNearestId(center, max_radius, allowed_ids);
    }

    bool SpatialStructure::Raycast(const Ray& ray, float& out_t, int& out_entity_id) const {
        return impl_->Raycast(ray, out_t, out_entity_id);
    }

    bool SpatialStructure::IsEmpty() const {
        return impl_->all_entities.empty();
    }

} // namespace Boidsish
