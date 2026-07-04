#pragma once

#include <vector>
#include <memory>
#include <functional>
#include <glm/glm.hpp>
#include "collision.h"

namespace Boidsish {

    class EntityBase;

    /**
     * @brief Encapsulates a bucketed spatial grid for spatial queries.
     *
     * Supports incremental updates, internal batching, and internal locking.
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
         * @brief Buffers an entity for addition to the spatial structure.
         */
        void BufferAdd(std::shared_ptr<EntityBase> entity);

        /**
         * @brief Buffers an entity ID for removal from the spatial structure.
         */
        void BufferRemove(int id);

        /**
         * @brief Buffers an entity for potential grid update.
         *
         * Thread-safe. Only queues an update if the entity crossed a cell boundary.
         */
        void BufferUpdate(std::shared_ptr<EntityBase> entity);

        /**
         * @brief Processes all buffered additions, removals, and moves.
         *
         * Applies changes to the internal grid state.
         */
        void ProcessBufferedUpdates();

        /**
         * @brief Clears the spatial structure.
         */
        void Clear();

        /**
         * @brief Finds all entities within a certain radius that pass a filter.
         */
        void QueryRadius(const glm::vec3& center, float radius, const std::function<void(std::shared_ptr<EntityBase>)>& callback) const;

        /**
         * @brief Finds the nearest entity that passes a filter.
         */
        std::shared_ptr<EntityBase> QueryNearest(const glm::vec3& center, float max_radius, const std::function<bool(std::shared_ptr<EntityBase>)>& filter) const;

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
