#pragma once

#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <GL/glew.h>
#include "shader.h"

namespace Boidsish {

    struct LBVHNode {
        glm::vec3 min_pt;
        int left;
        glm::vec3 max_pt;
        int right;
        int parent;
        int object_idx;
        int _pad[2];
    };

    struct LBVH_AABB {
        glm::vec3 min_pt;
        float _pad1;
        glm::vec3 max_pt;
        float _pad2;
    };

    class LBVHManager {
    public:
        LBVHManager();
        ~LBVHManager();

        /**
         * @brief Builds a full LBVH from a list of AABBs.
         *
         * @param aabbs The bounding boxes of the objects to include in the LBVH.
         * @param scene_min Minimum bounds of the scene for Morton code normalization.
         * @param scene_max Maximum bounds of the scene for Morton code normalization.
         */
        void Build(const std::vector<LBVH_AABB>& aabbs, glm::vec3 scene_min, glm::vec3 scene_max);

        /**
         * @brief Updates the AABBs of existing nodes without changing the tree structure.
         * Useful for moving objects that don't significantly change their relative topology.
         *
         * @param aabbs The updated bounding boxes (must match original count and order).
         */
        void Refit(const std::vector<LBVH_AABB>& aabbs);

        /**
         * @brief Bind the LBVH nodes SSBO to the specified binding point.
         */
        void Bind(GLuint binding = 20) const;

        GLuint GetNodesBuffer() const { return nodes_ssbo_; }
        int GetRootIndex() const { return 0; }
        int GetNumObjects() const { return num_objects_; }

    private:
        void _AllocateBuffers(int num_objects);
        void _CleanupBuffers();

        int num_objects_ = 0;

        // GPU Buffers
        GLuint nodes_ssbo_ = 0;
        GLuint aabb_ssbo_ = 0;
        GLuint morton_codes_ssbo_[2] = {0, 0}; // Double buffered for radix sort
        GLuint object_indices_ssbo_[2] = {0, 0};
        GLuint bit_counts_ssbo_ = 0;
        GLuint prefix_sums_ssbo_ = 0;
        GLuint refit_counters_ssbo_ = 0;

        // Shaders
        std::unique_ptr<ComputeShader> morton_shader_;
        std::unique_ptr<ComputeShader> prefix_sum_shader_;
        std::unique_ptr<ComputeShader> sort_step1_shader_;
        std::unique_ptr<ComputeShader> sort_step2_shader_;
        std::unique_ptr<ComputeShader> build_shader_;
        std::unique_ptr<ComputeShader> refit_shader_;
    };

} // namespace Boidsish
