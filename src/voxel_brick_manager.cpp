#include "voxel_brick_manager.h"
#include "logger.h"
#include <vector>
#include <numeric>
#include <cstring>

namespace Boidsish {

    VoxelBrickManager::VoxelBrickManager() {}

    VoxelBrickManager::~VoxelBrickManager() {
        if (brick_pool_sampling_tex_ != 0) glDeleteTextures(1, &brick_pool_sampling_tex_);
        if (metadata_ssbo_ != 0) glDeleteBuffers(1, &metadata_ssbo_);
        if (votes_ssbo_ != 0) glDeleteBuffers(1, &votes_ssbo_);
        if (free_list_ssbo_ != 0) glDeleteBuffers(1, &free_list_ssbo_);
        if (hash_table_ssbo_ != 0) glDeleteBuffers(1, &hash_table_ssbo_);
        if (accumulation_ssbo_ != 0) glDeleteBuffers(1, &accumulation_ssbo_);
        if (proposed_pos_ssbo_ != 0) glDeleteBuffers(1, &proposed_pos_ssbo_);
    }

    void VoxelBrickManager::Initialize() {
        if (initialized_) return;

        _CreateTextures();
        _CreateBuffers();
        _LoadShaders();

        initialized_ = true;
        logger::INFO("VoxelBrickManager initialized");
    }

    void VoxelBrickManager::Update(float delta_time, float time) {
        if (!initialized_) return;

        static int frame_index = 0;
        frame_index++;

        // 1. Run management shader to allocate/deallocate bricks based on votes
        manage_shader_->use();
        manage_shader_->setFloat("u_delta_time", delta_time);
        manage_shader_->setFloat("u_time", time);
        manage_shader_->setInt("u_frameIndex", frame_index);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::VoxelBrickMetadata(), metadata_ssbo_);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::VoxelBrickVotes(), votes_ssbo_);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::VoxelFreeList(), free_list_ssbo_);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::VoxelHashTable(), hash_table_ssbo_);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::VoxelProposedGridPos(), proposed_pos_ssbo_);

        glDispatchCompute((Constants::Class::VoxelBricks::HashTableSize() + 63) / 64, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // 2. Normalize and copy accumulated data to sampling texture
        copy_shader_->use();
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::VoxelAccumulation(), accumulation_ssbo_);
        glBindImageTexture(1, brick_pool_sampling_tex_, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);

        int pool_size = 16 * Constants::Class::VoxelBricks::BrickSize();
        glDispatchCompute(pool_size / 8, pool_size / 8, pool_size / 8);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    void VoxelBrickManager::ClearAccumulation() {
        if (!initialized_) return;

        // Run clear shader to reset accumulation buffer in parallel on GPU
        clear_shader_->use();
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::VoxelAccumulation(), accumulation_ssbo_);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::VoxelBrickVotes(), votes_ssbo_);

        // Dispatch enough threads to cover all data.
        // MaxBricks * 8*8*8 = 4096 * 512 = 2,097,152 entries.
        // We can do this brick-by-brick or entry-by-entry.
        glDispatchCompute((Constants::Class::VoxelBricks::MaxBricks() * 512 + 255) / 256, 1, 1);

        // Also clear votes SSBO (HashTableSize entries)
        // This is done in the same shader or another dispatch.
        // For simplicity, let's use another dispatch for votes if it's large.
        glDispatchCompute((Constants::Class::VoxelBricks::HashTableSize() + 255) / 256, 1, 1);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    void VoxelBrickManager::BindResources(int unit_offset) {
        if (!initialized_) return;

        // Bind sampling texture to the global voxel unit
        glActiveTexture(GL_TEXTURE0 + Constants::Class::VoxelBricks::BrickPoolUnit() + unit_offset);
        glBindTexture(GL_TEXTURE_3D, brick_pool_sampling_tex_);

        // Bind metadata and hash table for raymarching
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::VoxelBrickMetadata(), metadata_ssbo_);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::VoxelHashTable(), hash_table_ssbo_);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::VoxelBrickVotes(), votes_ssbo_);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::VoxelProposedGridPos(), proposed_pos_ssbo_);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::VoxelAccumulation(), accumulation_ssbo_);
    }

    void VoxelBrickManager::_CreateTextures() {
        int pool_size = 16 * Constants::Class::VoxelBricks::BrickSize();

        // Sampling: RGBA16F
        glGenTextures(1, &brick_pool_sampling_tex_);
        glBindTexture(GL_TEXTURE_3D, brick_pool_sampling_tex_);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, pool_size, pool_size, pool_size, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }

    void VoxelBrickManager::_CreateBuffers() {
        // Metadata: MaxBricks
        glGenBuffers(1, &metadata_ssbo_);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, metadata_ssbo_);
        std::vector<VoxelBrickMetadata> initial_metadata(Constants::Class::VoxelBricks::MaxBricks());
        for (auto& m : initial_metadata) m.grid_pos.w = -1; // Mark as inactive
        glBufferData(GL_SHADER_STORAGE_BUFFER, initial_metadata.size() * sizeof(VoxelBrickMetadata), initial_metadata.data(), GL_DYNAMIC_DRAW);

        // Votes: HashTableSize
        glGenBuffers(1, &votes_ssbo_);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, votes_ssbo_);
        glBufferData(GL_SHADER_STORAGE_BUFFER, Constants::Class::VoxelBricks::HashTableSize() * sizeof(int), nullptr, GL_DYNAMIC_DRAW);

        // Proposed Grid Pos: HashTableSize
        glGenBuffers(1, &proposed_pos_ssbo_);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, proposed_pos_ssbo_);
        glBufferData(GL_SHADER_STORAGE_BUFFER, Constants::Class::VoxelBricks::HashTableSize() * sizeof(glm::ivec4), nullptr, GL_DYNAMIC_DRAW);

        // Free list: MaxBricks
        glGenBuffers(1, &free_list_ssbo_);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, free_list_ssbo_);
        std::vector<int> initial_free_list(Constants::Class::VoxelBricks::MaxBricks() + 1);
        initial_free_list[0] = Constants::Class::VoxelBricks::MaxBricks(); // Count at index 0
        std::iota(initial_free_list.begin() + 1, initial_free_list.end(), 0);
        glBufferData(GL_SHADER_STORAGE_BUFFER, initial_free_list.size() * sizeof(int), initial_free_list.data(), GL_STATIC_DRAW);

        // Hash table: HashTableSize
        glGenBuffers(1, &hash_table_ssbo_);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, hash_table_ssbo_);
        std::vector<int> initial_hash_table(Constants::Class::VoxelBricks::HashTableSize(), -1);
        glBufferData(GL_SHADER_STORAGE_BUFFER, initial_hash_table.size() * sizeof(int), initial_hash_table.data(), GL_STATIC_DRAW);

        // Accumulation SSBO: MaxBricks * BrickSize^3 * 4 (density, dirX, dirY, dirZ)
        glGenBuffers(1, &accumulation_ssbo_);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, accumulation_ssbo_);
        size_t accum_size = Constants::Class::VoxelBricks::MaxBricks() * 8 * 8 * 8 * 4 * sizeof(GLuint);
        glBufferData(GL_SHADER_STORAGE_BUFFER, accum_size, nullptr, GL_DYNAMIC_DRAW);
    }

    void VoxelBrickManager::_LoadShaders() {
        manage_shader_ = std::make_unique<ComputeShader>("shaders/voxel_manage.comp");
        clear_shader_ = std::make_unique<ComputeShader>("shaders/voxel_clear.comp");
        copy_shader_ = std::make_unique<ComputeShader>("shaders/voxel_copy.comp");
    }

} // namespace Boidsish
