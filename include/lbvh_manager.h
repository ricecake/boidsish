#pragma once

#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <GL/glew.h>
#include "shader.h"

namespace Boidsish {

    struct LBVHNode {
        glm::vec4 min_pt_left;
        glm::vec4 max_pt_right;
        int parent;
        int object_idx;
        int _pad[2];
    };

    struct LBVH_AABB {
        glm::vec4 min_pt;
        glm::vec4 max_pt;
    };

    class LBVHManager {
    public:
        LBVHManager();
        ~LBVHManager();

        /**
         * @brief Builds a full LBVH from a list of AABBs and active flags.
         *
         * @param aabbs The bounding boxes of the objects.
         * @param active Active flags (0 = inactive, 1 = active).
         * @param scene_min Minimum bounds of the scene.
         * @param scene_max Maximum bounds of the scene.
         */
        void Build(const std::vector<LBVH_AABB>& aabbs, const std::vector<uint32_t>& active, glm::vec3 scene_min, glm::vec3 scene_max);

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
        GLuint active_ssbo_ = 0;
        GLuint morton_codes_ssbo_[2] = {0, 0}; // Double buffered for radix sort
        GLuint object_indices_ssbo_[2] = {0, 0};
        GLuint bit_counts_ssbo_ = 0;
        GLuint prefix_sums_ssbo_ = 0;
        GLuint block_sums_ssbo_ = 0;
        GLuint total_zeros_ssbo_ = 0;
        GLuint refit_counters_ssbo_ = 0;

        // Shaders
        std::unique_ptr<ComputeShader> morton_shader_;
        std::unique_ptr<ComputeShader> prefix_sum_shader_;
        std::unique_ptr<ComputeShader> prefix_sum_blocks_shader_;
        std::unique_ptr<ComputeShader> prefix_sum_add_shader_;
        std::unique_ptr<ComputeShader> sort_step1_shader_;
        std::unique_ptr<ComputeShader> sort_step2_shader_;
        std::unique_ptr<ComputeShader> build_shader_;
        std::unique_ptr<ComputeShader> refit_shader_;
    };

} // namespace Boidsish
