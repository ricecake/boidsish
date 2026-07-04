#pragma once

#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "collision.h"

namespace Boidsish {

    class EntityBase;

    /**
     * @brief Encapsulates a bucketed spatial grid for spatial queries.
     *
     * Supports incremental updates and internal batching to avoid full rebuilds.
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
         * @brief Adds an entity to the spatial structure.
         */
        void AddEntity(std::shared_ptr<EntityBase> entity);

        /**
         * @brief Removes an entity from the spatial structure by ID.
         */
        void RemoveEntity(int id);

        /**
         * @brief Buffers an entity for potential grid update.
         *
         * Thread-safe. Detects if the entity has moved to a new cell.
         */
        void BufferUpdate(std::shared_ptr<EntityBase> entity);

        /**
         * @brief Processes all buffered updates and applies them to the grid.
         *
         * Should be called in a serial phase.
         */
        void ProcessBufferedUpdates();

        /**
         * @brief Clears the spatial structure.
         */
        void Clear();

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
