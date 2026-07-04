#pragma once

#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "collision.h"

namespace Boidsish {

    class EntityBase;

    /**
     * @brief Encapsulates a bucketed spatial grid for spatial queries.
     */
    class SpatialStructure {
    public:
        SpatialStructure();
        ~SpatialStructure();

        // Non-copyable
        SpatialStructure(const SpatialStructure&) = delete;
        SpatialStructure& operator=(const SpatialStructure&) = delete;

        // Movable
        SpatialStructure(SpatialStructure&&) noexcept;
        SpatialStructure& operator=(SpatialStructure&&) noexcept;

        void swap(SpatialStructure& other) noexcept;

        /**
         * @brief Updates the spatial structure from a list of entities.
         */
        void Update(const std::vector<std::shared_ptr<EntityBase>>& entities);

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
