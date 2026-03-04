#include "lbvh_manager.h"
#include <algorithm>
#include <iostream>

namespace Boidsish {

LBVHManager::LBVHManager() {
    morton_shader_ = std::make_unique<ComputeShader>("shaders/lbvh/lbvh_morton.comp");
    prefix_sum_shader_ = std::make_unique<ComputeShader>("shaders/lbvh/lbvh_prefix_sum.comp");
    sort_step1_shader_ = std::make_unique<ComputeShader>("shaders/lbvh/lbvh_sort_step1.comp");
    sort_step2_shader_ = std::make_unique<ComputeShader>("shaders/lbvh/lbvh_sort_step2.comp");
    build_shader_ = std::make_unique<ComputeShader>("shaders/lbvh/lbvh_build.comp");
    refit_shader_ = std::make_unique<ComputeShader>("shaders/lbvh/lbvh_refit.comp");
}

LBVHManager::~LBVHManager() {
    _CleanupBuffers();
}

void LBVHManager::_AllocateBuffers(int num_objects) {
    if (num_objects_ == num_objects) return;

    _CleanupBuffers();
    num_objects_ = num_objects;
    if (num_objects <= 0) return;

    int num_internal = std::max(1, num_objects - 1);
    int num_nodes = num_objects + num_internal;

    // Nodes buffer
    glGenBuffers(1, &nodes_ssbo_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, nodes_ssbo_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, num_nodes * sizeof(LBVHNode), nullptr, GL_DYNAMIC_DRAW);

    // Input AABBs buffer
    glGenBuffers(1, &aabb_ssbo_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, aabb_ssbo_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, num_objects * sizeof(LBVH_AABB), nullptr, GL_DYNAMIC_DRAW);

    // Morton codes and indices (double buffered for sorting)
    glGenBuffers(2, morton_codes_ssbo_);
    glGenBuffers(2, object_indices_ssbo_);
    for(int i=0; i<2; ++i) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, morton_codes_ssbo_[i]);
        glBufferData(GL_SHADER_STORAGE_BUFFER, num_objects * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, object_indices_ssbo_[i]);
        glBufferData(GL_SHADER_STORAGE_BUFFER, num_objects * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);
    }

    // Sorting/Prefix Sum helpers
    glGenBuffers(1, &bit_counts_ssbo_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bit_counts_ssbo_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, num_objects * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &prefix_sums_ssbo_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, prefix_sums_ssbo_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, num_objects * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);

    // Refit counters
    glGenBuffers(1, &refit_counters_ssbo_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, refit_counters_ssbo_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, num_internal * sizeof(int), nullptr, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void LBVHManager::_CleanupBuffers() {
    if (nodes_ssbo_) glDeleteBuffers(1, &nodes_ssbo_);
    if (aabb_ssbo_) glDeleteBuffers(1, &aabb_ssbo_);
    if (morton_codes_ssbo_[0]) glDeleteBuffers(2, morton_codes_ssbo_);
    if (object_indices_ssbo_[0]) glDeleteBuffers(2, object_indices_ssbo_);
    if (bit_counts_ssbo_) glDeleteBuffers(1, &bit_counts_ssbo_);
    if (prefix_sums_ssbo_) glDeleteBuffers(1, &prefix_sums_ssbo_);
    if (refit_counters_ssbo_) glDeleteBuffers(1, &refit_counters_ssbo_);

    nodes_ssbo_ = 0;
    aabb_ssbo_ = 0;
    morton_codes_ssbo_[0] = morton_codes_ssbo_[1] = 0;
    object_indices_ssbo_[0] = object_indices_ssbo_[1] = 0;
    bit_counts_ssbo_ = 0;
    prefix_sums_ssbo_ = 0;
    refit_counters_ssbo_ = 0;
}

void LBVHManager::Build(const std::vector<LBVH_AABB>& aabbs, glm::vec3 scene_min, glm::vec3 scene_max) {
    int n = static_cast<int>(aabbs.size());
    if (n <= 0) return;
    _AllocateBuffers(n);

    // 1. Upload AABBs
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, aabb_ssbo_);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, n * sizeof(LBVH_AABB), aabbs.data());

    // 2. Generate Morton Codes
    morton_shader_->use();
    morton_shader_->setInt("u_numObjects", n);
    morton_shader_->setVec3("u_sceneMin", scene_min);
    morton_shader_->setVec3("u_sceneMax", scene_max);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, aabb_ssbo_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, morton_codes_ssbo_[0]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, object_indices_ssbo_[0]);
    glDispatchCompute((n + 255) / 256, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // 3. Parallel Radix Sort (30 bits)
    int src = 0;
    for (int bit = 0; bit < 30; ++bit) {
        int dst = 1 - src;

        // Pass 1: Count bits
        sort_step1_shader_->use();
        sort_step1_shader_->setInt("u_numElements", n);
        sort_step1_shader_->setInt("u_bitOffset", bit);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, morton_codes_ssbo_[src]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, bit_counts_ssbo_);
        glDispatchCompute((n + 255) / 256, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // Parallel Prefix Sum (exclusive) on bit_counts
        prefix_sum_shader_->use();
        prefix_sum_shader_->setUint("u_numElements", n);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bit_counts_ssbo_);
        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        uint32_t total_zeros = 0;
        uint32_t last_prefix, last_count;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, bit_counts_ssbo_);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, (n-1)*sizeof(uint32_t), sizeof(uint32_t), &last_prefix);

        uint32_t last_code;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, morton_codes_ssbo_[src]);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, (n-1)*sizeof(uint32_t), sizeof(uint32_t), &last_code);
        last_count = ((last_code >> bit) & 1u) == 0u ? 1u : 0u;
        total_zeros = last_prefix + last_count;

        // Pass 2: Scatter
        sort_step2_shader_->use();
        sort_step2_shader_->setInt("u_numElements", n);
        sort_step2_shader_->setInt("u_bitOffset", bit);
        sort_step2_shader_->setUint("u_totalZeros", total_zeros);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, morton_codes_ssbo_[src]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, morton_codes_ssbo_[dst]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, object_indices_ssbo_[src]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, object_indices_ssbo_[dst]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, bit_counts_ssbo_);
        glDispatchCompute((n + 255) / 256, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        src = dst;
    }

    // 4. Build Tree
    build_shader_->use();
    build_shader_->setInt("u_numObjects", n);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, morton_codes_ssbo_[src]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, object_indices_ssbo_[src]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 20, nodes_ssbo_);
    glDispatchCompute((2 * n - 1 + 255) / 256, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // 5. Refit AABBs (Initialize and propagate)
    Refit(aabbs);
}

void LBVHManager::Refit(const std::vector<LBVH_AABB>& aabbs) {
    int n = num_objects_;
    if (n <= 0) return;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, aabb_ssbo_);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, n * sizeof(LBVH_AABB), aabbs.data());

    int num_internal = std::max(1, n - 1);
    std::vector<int> zeros(num_internal, 0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, refit_counters_ssbo_);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, num_internal * sizeof(int), zeros.data());

    refit_shader_->use();
    refit_shader_->setInt("u_numObjects", n);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, aabb_ssbo_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, refit_counters_ssbo_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 20, nodes_ssbo_);
    glDispatchCompute((n + 255) / 256, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void LBVHManager::Bind(GLuint binding) const {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, nodes_ssbo_);
}

} // namespace Boidsish
