#include "lbvh_manager.h"
#include <algorithm>
#include <iostream>

namespace Boidsish {

LBVHManager::LBVHManager() {
    morton_shader_ = std::make_unique<ComputeShader>("shaders/lbvh/lbvh_morton.comp");
    prefix_sum_shader_ = std::make_unique<ComputeShader>("shaders/lbvh/lbvh_prefix_sum.comp");
    prefix_sum_blocks_shader_ = std::make_unique<ComputeShader>("shaders/lbvh/lbvh_prefix_sum_blocks.comp");
    prefix_sum_add_shader_ = std::make_unique<ComputeShader>("shaders/lbvh/lbvh_prefix_sum_add.comp");
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

    glGenBuffers(1, &nodes_ssbo_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, nodes_ssbo_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, num_nodes * sizeof(LBVHNode), nullptr, GL_DYNAMIC_DRAW);
    uint32_t zero = 0;
    glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);

    glGenBuffers(1, &aabb_ssbo_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, aabb_ssbo_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, num_objects * sizeof(LBVH_AABB), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &active_ssbo_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, active_ssbo_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, num_objects * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(2, morton_codes_ssbo_);
    glGenBuffers(2, object_indices_ssbo_);
    for(int i=0; i<2; ++i) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, morton_codes_ssbo_[i]);
        glBufferData(GL_SHADER_STORAGE_BUFFER, num_objects * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, object_indices_ssbo_[i]);
        glBufferData(GL_SHADER_STORAGE_BUFFER, num_objects * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);
    }

    glGenBuffers(1, &bit_counts_ssbo_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bit_counts_ssbo_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, num_objects * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &prefix_sums_ssbo_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, prefix_sums_ssbo_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, num_objects * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);

    int num_blocks = (num_objects + 511) / 512;
    glGenBuffers(1, &block_sums_ssbo_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, block_sums_ssbo_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, num_blocks * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &total_zeros_ssbo_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, total_zeros_ssbo_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &refit_counters_ssbo_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, refit_counters_ssbo_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, num_internal * sizeof(int), nullptr, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void LBVHManager::_CleanupBuffers() {
    if (nodes_ssbo_) glDeleteBuffers(1, &nodes_ssbo_);
    if (aabb_ssbo_) glDeleteBuffers(1, &aabb_ssbo_);
    if (active_ssbo_) glDeleteBuffers(1, &active_ssbo_);
    if (morton_codes_ssbo_[0]) glDeleteBuffers(2, morton_codes_ssbo_);
    if (object_indices_ssbo_[0]) glDeleteBuffers(2, object_indices_ssbo_);
    if (bit_counts_ssbo_) glDeleteBuffers(1, &bit_counts_ssbo_);
    if (prefix_sums_ssbo_) glDeleteBuffers(1, &prefix_sums_ssbo_);
    if (block_sums_ssbo_) glDeleteBuffers(1, &block_sums_ssbo_);
    if (total_zeros_ssbo_) glDeleteBuffers(1, &total_zeros_ssbo_);
    if (refit_counters_ssbo_) glDeleteBuffers(1, &refit_counters_ssbo_);

    nodes_ssbo_ = 0;
    aabb_ssbo_ = 0;
    active_ssbo_ = 0;
    morton_codes_ssbo_[0] = morton_codes_ssbo_[1] = 0;
    object_indices_ssbo_[0] = object_indices_ssbo_[1] = 0;
    bit_counts_ssbo_ = 0;
    prefix_sums_ssbo_ = 0;
    block_sums_ssbo_ = 0;
    total_zeros_ssbo_ = 0;
    refit_counters_ssbo_ = 0;
}

void LBVHManager::Build(const std::vector<LBVH_AABB>& aabbs, const std::vector<uint32_t>& active, glm::vec3 scene_min, glm::vec3 scene_max) {
    int n = static_cast<int>(aabbs.size());
    if (n <= 0) return;
    _AllocateBuffers(n);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, aabb_ssbo_);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, n * sizeof(LBVH_AABB), aabbs.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, active_ssbo_);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, n * sizeof(uint32_t), active.data());

    _BuildGPU(n, scene_min, scene_max);
}

void LBVHManager::Build(GLuint external_aabb_ssbo, GLuint external_active_ssbo, int n, glm::vec3 scene_min, glm::vec3 scene_max) {
    if (n <= 0) return;
    _AllocateBuffers(n);

    // Copy from external buffers to internal buffers
    glBindBuffer(GL_COPY_READ_BUFFER, external_aabb_ssbo);
    glBindBuffer(GL_COPY_WRITE_BUFFER, aabb_ssbo_);
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, n * sizeof(LBVH_AABB));

    glBindBuffer(GL_COPY_READ_BUFFER, external_active_ssbo);
    glBindBuffer(GL_COPY_WRITE_BUFFER, active_ssbo_);
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, n * sizeof(uint32_t));

    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

    _BuildGPU(n, scene_min, scene_max);
}

void LBVHManager::_BuildGPU(int n, glm::vec3 scene_min, glm::vec3 scene_max) {
    morton_shader_->use();
    morton_shader_->setInt("u_numObjects", n);
    morton_shader_->setVec3("u_sceneMin", scene_min);
    morton_shader_->setVec3("u_sceneMax", scene_max);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, aabb_ssbo_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, morton_codes_ssbo_[0]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, object_indices_ssbo_[0]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, active_ssbo_);
    glDispatchCompute((n + 255) / 256, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

    int src = 0;
    for (int bit = 0; bit < 30; ++bit) {
        int dst = 1 - src;

        sort_step1_shader_->use();
        sort_step1_shader_->setInt("u_numElements", n);
        sort_step1_shader_->setInt("u_bitOffset", bit);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, morton_codes_ssbo_[src]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, bit_counts_ssbo_);
        glDispatchCompute((n + 255) / 256, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // Scalable Prefix Sum
        int num_blocks = (n + 511) / 512;
        prefix_sum_blocks_shader_->use();
        prefix_sum_blocks_shader_->setUint("u_numElements", n);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bit_counts_ssbo_);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, block_sums_ssbo_);
        glDispatchCompute(num_blocks, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        if (num_blocks > 1) {
            // Scan the block sums (reusing the simple shader as long as num_blocks <= 512)
            prefix_sum_shader_->use();
            prefix_sum_shader_->setUint("u_numElements", num_blocks);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, block_sums_ssbo_);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, total_zeros_ssbo_);
            glDispatchCompute(1, 1, 1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

            // Add back to individual elements
            prefix_sum_add_shader_->use();
            prefix_sum_add_shader_->setUint("u_numElements", n);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bit_counts_ssbo_);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, block_sums_ssbo_);
            glDispatchCompute(num_blocks, 1, 1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        } else {
            // Single block - total sum is already in block_sums_ssbo_[0]
            // Copy it to total_zeros_ssbo_ for sort_step2
            glBindBuffer(GL_COPY_READ_BUFFER, block_sums_ssbo_);
            glBindBuffer(GL_COPY_WRITE_BUFFER, total_zeros_ssbo_);
            glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, sizeof(uint32_t));
            glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
        }

        sort_step2_shader_->use();
        sort_step2_shader_->setInt("u_numElements", n);
        sort_step2_shader_->setInt("u_bitOffset", bit);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, morton_codes_ssbo_[src]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, morton_codes_ssbo_[dst]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, object_indices_ssbo_[src]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, object_indices_ssbo_[dst]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, bit_counts_ssbo_);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, total_zeros_ssbo_);
        glDispatchCompute((n + 255) / 256, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        src = dst;
    }

    build_shader_->use();
    build_shader_->setInt("u_numObjects", n);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, morton_codes_ssbo_[src]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, object_indices_ssbo_[src]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 20, nodes_ssbo_);
    glDispatchCompute((2 * n - 1 + 255) / 256, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Initial Refit to compute bounds from leaves to root
    int num_internal = std::max(1, n - 1);
    uint32_t zero = 0;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, refit_counters_ssbo_);
    glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

    refit_shader_->use();
    refit_shader_->setInt("u_numObjects", n);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, aabb_ssbo_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, refit_counters_ssbo_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 20, nodes_ssbo_);
    glDispatchCompute((n + 255) / 256, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void LBVHManager::Refit(const std::vector<LBVH_AABB>& aabbs) {
    int n = num_objects_;
    if (n <= 0) return;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, aabb_ssbo_);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, n * sizeof(LBVH_AABB), aabbs.data());

    int num_internal = std::max(1, n - 1);
    uint32_t zero = 0;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, refit_counters_ssbo_);
    glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

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
