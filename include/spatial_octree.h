#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <functional>

namespace Boidsish {

    /**
     * @brief A Sparse Spatial Octree for storing density fields (scent trails, volumetric clouds).
     * Supports diffusion, decay, and drift.
     */
    class SpatialOctree {
    public:
        struct Node {
            float density = 0.0f;
            std::unique_ptr<Node> children[8];
            bool is_leaf = true;

            Node() {
                for (int i = 0; i < 8; ++i) children[i] = nullptr;
            }
        };

        /**
         * @brief Construct a new Spatial Octree.
         *
         * @param center The world-space center of the octree.
         * @param size The side length of the octree's root cube.
         * @param max_depth The maximum subdivision depth (leaf resolution).
         */
        SpatialOctree(const glm::vec3& center, float size, int max_depth);

        /**
         * @brief Add density at a specific world-space position.
         *
         * @param pos World-space position.
         * @param amount Amount of density to add.
         */
        void AddDensity(const glm::vec3& pos, float amount);

        /**
         * @brief Sample the density at a specific world-space position.
         *
         * @param pos World-space position.
         * @return The density value at that position.
         */
        float Sample(const glm::vec3& pos) const;

        /**
         * @brief Get the gradient of the density field at a position.
         * Useful for entities following the trail.
         *
         * @param pos World-space position.
         * @return Gradient vector.
         */
        glm::vec3 GetGradient(const glm::vec3& pos) const;

        /**
         * @brief Update the octree physics (diffusion, decay, drift).
         *
         * @param dt Delta time.
         * @param diffusion_rate Rate at which density spreads to neighbors.
         * @param decay_rate Rate at which density fades over time.
         * @param drift World-space velocity vector for advection.
         */
        void Update(float dt, float diffusion_rate, float decay_rate, const glm::vec3& drift = glm::vec3(0.0f));

        /**
         * @brief Traverse the octree and call a function for each leaf node with non-zero density.
         *
         * @param callback Function receiving leaf bounds (min, max) and density.
         */
        void Traverse(std::function<void(const glm::vec3& min, const glm::vec3& max, float density)> callback) const;

        /**
         * @brief Get the world-space size of a leaf voxel.
         */
        float GetVoxelSize() const;

        // Getters
        int GetMaxDepth() const { return max_depth_; }
        float GetSize() const { return size_; }
        glm::vec3 GetCenter() const { return center_; }

    private:
        std::unique_ptr<Node> root_;
        glm::vec3 center_;
        float size_;
        int max_depth_;

        // Internal recursive helpers
        void AddDensityRecursive(Node* node, const glm::vec3& min, const glm::vec3& max, int depth, const glm::vec3& pos, float amount);
        float SampleRecursive(Node* node, const glm::vec3& min, const glm::vec3& max, int depth, const glm::vec3& pos) const;
        void TraverseRecursive(Node* node, const glm::vec3& min, const glm::vec3& max, int depth, std::function<void(const glm::vec3&, const glm::vec3&, float)> callback) const;

        void Subdivide(Node* node);
        int GetChildIndex(const glm::vec3& min, const glm::vec3& max, const glm::vec3& pos) const;
        void GetChildBounds(const glm::vec3& min, const glm::vec3& max, int index, glm::vec3& child_min, glm::vec3& child_max) const;

        // Physics implementation
        void ApplyDiffusionAndDecay(float dt, float diffusion, float decay);
        void ApplyDrift(float dt, const glm::vec3& drift);
    };

} // namespace Boidsish
