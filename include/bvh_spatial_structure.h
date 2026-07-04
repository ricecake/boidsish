#pragma once

#include <vector>
#include <memory>
#include <typeindex>
#include <glm/glm.hpp>
#include "collision.h"

namespace task_thread_pool {
    class task_thread_pool;
}

namespace Boidsish {

    class EntityBase;

    /**
     * @brief Encapsulates a Bounding Volume Hierarchy for spatial queries.
     *
     * This class separates the low-level BVH implementation from the entity management.
     */
    class BvhSpatialStructure {
    public:
        BvhSpatialStructure();
        ~BvhSpatialStructure();

        // Non-copyable
        BvhSpatialStructure(const BvhSpatialStructure&) = delete;
        BvhSpatialStructure& operator=(const BvhSpatialStructure&) = delete;

        // Movable
        BvhSpatialStructure(BvhSpatialStructure&&) noexcept;
        BvhSpatialStructure& operator=(BvhSpatialStructure&&) noexcept;

        void swap(BvhSpatialStructure& other) noexcept {
            std::swap(impl_, other.impl_);
        }

        /**
         * @brief Updates the BVH from a list of entities.
         *
         * Automatically decides between a full rebuild and a faster refit
         * based on whether the set of entities has changed.
         */
        void Update(const std::vector<std::shared_ptr<EntityBase>>& entities, task_thread_pool::task_thread_pool& pool);

        /**
         * @brief Finds all entities within a certain radius.
         */
        std::vector<std::shared_ptr<EntityBase>> GetEntitiesInRadius(const glm::vec3& center, float radius) const;

        /**
         * @brief Finds the nearest entity.
         */
        std::shared_ptr<EntityBase> FindNearest(const glm::vec3& center, float max_radius) const;

        /**
         * @brief Raycasts against the entity AABBs.
         */
        bool Raycast(const Ray& ray, float& out_t, int& out_entity_id) const;

        /**
         * @brief Checks if the structure is empty.
         */
        bool IsEmpty() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace Boidsish
