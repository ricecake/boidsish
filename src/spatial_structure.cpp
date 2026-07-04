#include "spatial_structure.h"
#include "entity.h"
#include "constants.h"
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <vector>
#include <mutex>
#include <shared_mutex>

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

    struct EntityInfo {
        GridKey min_k, max_k;
        std::shared_ptr<EntityBase> entity;
    };

    struct SpatialStructure::Impl {
        float cell_size = 32.0f;
        std::unordered_map<GridKey, std::vector<int>, GridKeyHash> grid;
        std::unordered_map<int, EntityInfo> entity_tracking;
        std::unordered_map<int, std::shared_ptr<EntityBase>> all_entities;

        mutable std::shared_mutex mutex;

        std::vector<std::shared_ptr<EntityBase>> add_buffer;
        std::vector<int> remove_buffer;
        std::vector<std::shared_ptr<EntityBase>> update_buffer;
        std::mutex buffer_mutex;

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

        void AddToGrid(int id, const GridKey& min_k, const GridKey& max_k) {
            for(int x = min_k.x; x <= max_k.x; ++x) {
                for(int y = min_k.y; y <= max_k.y; ++y) {
                    for(int z = min_k.z; z <= max_k.z; ++z) {
                        grid[{x, y, z}].push_back(id);
                    }
                }
            }
        }

        void RemoveFromGrid(int id, const GridKey& min_k, const GridKey& max_k) {
            for(int x = min_k.x; x <= max_k.x; ++x) {
                for(int y = min_k.y; y <= max_k.y; ++y) {
                    for(int z = min_k.z; z <= max_k.z; ++z) {
                        auto& cell = grid[{x, y, z}];
                        cell.erase(std::remove(cell.begin(), cell.end(), id), cell.end());
                        if (cell.empty()) grid.erase({x, y, z});
                    }
                }
            }
        }

        void InternalAdd(std::shared_ptr<EntityBase> entity) {
            int id = entity->GetId();
            if (all_entities.count(id)) return;

            all_entities[id] = entity;
            glm::vec3 pos = entity->GetPosition().Toglm();
            float half_size = entity->GetSize() * 0.5f;
            GridKey min_k = GetKey(pos - glm::vec3(half_size));
            GridKey max_k = GetKey(pos + glm::vec3(half_size));

            entity_tracking[id] = {min_k, max_k, entity};
            AddToGrid(id, min_k, max_k);
        }

        void InternalRemove(int id) {
            auto it = entity_tracking.find(id);
            if (it != entity_tracking.end()) {
                RemoveFromGrid(id, it->second.min_k, it->second.max_k);
                entity_tracking.erase(it);
            }
            all_entities.erase(id);
        }

        void BufferAdd(std::shared_ptr<EntityBase> entity) {
            std::lock_guard<std::mutex> lock(buffer_mutex);
            add_buffer.push_back(entity);
        }

        void BufferRemove(int id) {
            std::lock_guard<std::mutex> lock(buffer_mutex);
            remove_buffer.push_back(id);
        }

        void BufferUpdate(std::shared_ptr<EntityBase> entity) {
            int id = entity->GetId();

            {
                std::shared_lock lock(mutex);
                auto it = entity_tracking.find(id);
                if (it != entity_tracking.end()) {
                    glm::vec3 pos = entity->GetPosition().Toglm();
                    float half_size = entity->GetSize() * 0.5f;
                    GridKey new_min = GetKey(pos - glm::vec3(half_size));
                    GridKey new_max = GetKey(pos + glm::vec3(half_size));

                    // Only queue if it crossed cell boundary
                    if (new_min == it->second.min_k && new_max == it->second.max_k) {
                        return;
                    }
                }
            }

            std::lock_guard<std::mutex> lock(buffer_mutex);
            update_buffer.push_back(entity);
        }

        void ProcessBufferedUpdates() {
            std::vector<std::shared_ptr<EntityBase>> current_add;
            std::vector<int> current_remove;
            std::vector<std::shared_ptr<EntityBase>> current_update;

            {
                std::lock_guard<std::mutex> lock(buffer_mutex);
                current_add = std::move(add_buffer);
                current_remove = std::move(remove_buffer);
                current_update = std::move(update_buffer);
                add_buffer.clear();
                remove_buffer.clear();
                update_buffer.clear();
            }

            if (current_add.empty() && current_remove.empty() && current_update.empty()) {
                return;
            }

            std::unique_lock lock(mutex);

            for (const auto& id : current_remove) {
                InternalRemove(id);
            }

            for (const auto& entity : current_add) {
                InternalAdd(entity);
            }

            for (const auto& entity : current_update) {
                int id = entity->GetId();
                auto it = entity_tracking.find(id);
                if (it == entity_tracking.end()) {
                    InternalAdd(entity);
                    continue;
                }

                glm::vec3 pos = entity->GetPosition().Toglm();
                float half_size = entity->GetSize() * 0.5f;
                GridKey new_min = GetKey(pos - glm::vec3(half_size));
                GridKey new_max = GetKey(pos + glm::vec3(half_size));

                if (!(new_min == it->second.min_k && new_max == it->second.max_k)) {
                    RemoveFromGrid(id, it->second.min_k, it->second.max_k);
                    AddToGrid(id, new_min, new_max);
                    it->second.min_k = new_min;
                    it->second.max_k = new_max;
                }
            }
        }

        void Clear() {
            std::unique_lock lock(mutex);
            grid.clear();
            entity_tracking.clear();
            all_entities.clear();
            std::lock_guard<std::mutex> buffer_lock(buffer_mutex);
            add_buffer.clear();
            remove_buffer.clear();
            update_buffer.clear();
        }

        void QueryRadius(const glm::vec3& center, float radius, const std::function<void(std::shared_ptr<EntityBase>)>& callback) const {
            std::shared_lock lock(mutex);
            float radius_sq = radius * radius;

            if (radius > cell_size * 16.0f) {
                 for (const auto& [id, entity] : all_entities) {
                    glm::vec3 diff = entity->GetPosition().Toglm() - center;
                    if (glm::dot(diff, diff) <= radius_sq) {
                        callback(entity);
                    }
                }
                return;
            }

            GridKey min_k = GetKey(center - glm::vec3(radius));
            GridKey max_k = GetKey(center + glm::vec3(radius));

            std::unordered_set<int> found_ids;

            for(int x = min_k.x; x <= max_k.x; ++x) {
                for(int y = min_k.y; y <= max_k.y; ++y) {
                    for(int z = min_k.z; z <= max_k.z; ++z) {
                        auto it = grid.find({x, y, z});
                        if (it != grid.end()) {
                            for (int id : it->second) {
                                if (found_ids.count(id)) continue;
                                found_ids.insert(id);

                                auto ent_it = all_entities.find(id);
                                if (ent_it == all_entities.end()) continue;

                                glm::vec3 diff = ent_it->second->GetPosition().Toglm() - center;
                                if (glm::dot(diff, diff) <= radius_sq) {
                                    callback(ent_it->second);
                                }
                            }
                        }
                    }
                }
            }
        }

        std::shared_ptr<EntityBase> QueryNearest(const glm::vec3& center, float max_radius, const std::function<bool(std::shared_ptr<EntityBase>)>& filter) const {
            std::shared_lock lock(mutex);
            float nearest_dist_sq = max_radius * max_radius;
            std::shared_ptr<EntityBase> nearest_entity = nullptr;

            if (max_radius > cell_size * 16.0f) {
                for (const auto& [id, entity] : all_entities) {
                    if (filter && !filter(entity)) continue;

                    glm::vec3 diff = entity->GetPosition().Toglm() - center;
                    float dist_sq = glm::dot(diff, diff);
                    if (dist_sq < nearest_dist_sq) {
                        nearest_dist_sq = dist_sq;
                        nearest_entity = entity;
                    }
                }
                return nearest_entity;
            }

            GridKey min_k = GetKey(center - glm::vec3(max_radius));
            GridKey max_k = GetKey(center + glm::vec3(max_radius));

            std::unordered_set<int> checked_ids;

            for(int x = min_k.x; x <= max_k.x; ++x) {
                for(int y = min_k.y; y <= max_k.y; ++y) {
                    for(int z = min_k.z; z <= max_k.z; ++z) {
                        auto it = grid.find({x, y, z});
                        if (it != grid.end()) {
                            for (int id : it->second) {
                                if (checked_ids.count(id)) continue;
                                checked_ids.insert(id);

                                auto ent_it = all_entities.find(id);
                                if (ent_it == all_entities.end()) continue;
                                if (filter && !filter(ent_it->second)) continue;

                                glm::vec3 diff = ent_it->second->GetPosition().Toglm() - center;
                                float dist_sq = glm::dot(diff, diff);
                                if (dist_sq < nearest_dist_sq) {
                                    nearest_dist_sq = dist_sq;
                                    nearest_entity = ent_it->second;
                                }
                            }
                        }
                    }
                }
            }
            return nearest_entity;
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
            std::shared_lock lock(mutex);
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
            while (t < max_ray_dist) {
                auto it = grid.find({cell.x, cell.y, cell.z});
                if (it != grid.end()) {
                    for (int id : it->second) {
                        if (candidate_ids.find(id) == candidate_ids.end()) {
                            auto ent_it = all_entities.find(id);
                            if (ent_it != all_entities.end()) {
                                candidates.push_back(ent_it->second);
                                candidate_ids.insert(id);
                            }
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

    void SpatialStructure::BufferAdd(std::shared_ptr<EntityBase> entity) {
        impl_->BufferAdd(entity);
    }

    void SpatialStructure::BufferRemove(int id) {
        impl_->BufferRemove(id);
    }

    void SpatialStructure::BufferUpdate(std::shared_ptr<EntityBase> entity) {
        impl_->BufferUpdate(entity);
    }

    void SpatialStructure::ProcessBufferedUpdates() {
        impl_->ProcessBufferedUpdates();
    }

    void SpatialStructure::Clear() {
        impl_->Clear();
    }

    void SpatialStructure::QueryRadius(const glm::vec3& center, float radius, const std::function<void(std::shared_ptr<EntityBase>)>& callback) const {
        impl_->QueryRadius(center, radius, callback);
    }

    std::shared_ptr<EntityBase> SpatialStructure::QueryNearest(const glm::vec3& center, float max_radius, const std::function<bool(std::shared_ptr<EntityBase>)>& filter) const {
        return impl_->QueryNearest(center, max_radius, filter);
    }

    bool SpatialStructure::Raycast(const Ray& ray, float& out_t, int& out_entity_id) const {
        return impl_->Raycast(ray, out_t, out_entity_id);
    }

    bool SpatialStructure::IsEmpty() const {
        std::shared_lock lock(impl_->mutex);
        return impl_->all_entities.empty();
    }

} // namespace Boidsish
