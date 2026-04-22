#pragma once

#include <vector>
#include <memory>
#include <typeindex>
#include <glm/glm.hpp>
#include "collision.h"

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
         * @brief Rebuilds the BVH from a list of entities.
         * This should be called once per frame.
         */
        void Rebuild(const std::vector<std::shared_ptr<EntityBase>>& entities);

        /**
         * @brief Finds all entities within a certain radius.
         */
        std::vector<int> GetEntityIdsInRadius(const glm::vec3& center, float radius, const std::vector<int>& allowed_ids) const;

        /**
         * @brief Finds the nearest entity from an allowed set.
         */
        int FindNearestId(const glm::vec3& center, float max_radius, const std::vector<int>& allowed_ids) const;

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
