#include "voxel_brick_manager.h"
#include "logger.h"
#include <algorithm>
#include <numeric>

namespace Boidsish {

VoxelBrickManager::VoxelBrickManager() {}

VoxelBrickManager::~VoxelBrickManager() {
    if (brick_votes_buffer_) glDeleteBuffers(1, &brick_votes_buffer_);
    if (free_list_buffer_) glDeleteBuffers(1, &free_list_buffer_);
    if (brick_metadata_buffer_) glDeleteBuffers(1, &brick_metadata_buffer_);
    if (brick_pool_texture_) glDeleteTextures(1, &brick_pool_texture_);
    if (brick_pool_atomic_texture_) glDeleteTextures(1, &brick_pool_atomic_texture_);
}

void VoxelBrickManager::Initialize() {
    if (initialized_) return;

    _SetupBuffers();
    _SetupTextures();

    management_shader_ = std::make_unique<ComputeShader>("shaders/voxel_manage.comp");
    if (!management_shader_->isValid()) {
        logger::ERROR("Failed to compile voxel management shader");
    }

    clear_shader_ = std::make_unique<ComputeShader>("shaders/voxel_clear.comp");
    if (!clear_shader_->isValid()) {
        logger::ERROR("Failed to compile voxel clear shader");
    }

    copy_shader_ = std::make_unique<ComputeShader>("shaders/voxel_copy.comp");
    if (!copy_shader_->isValid()) {
        logger::ERROR("Failed to compile voxel copy shader");
    }

    initialized_ = true;
    logger::INFO("VoxelBrickManager initialized with %d max bricks", kMaxBricks);
}

void VoxelBrickManager::_SetupBuffers() {
    // Brick Votes
    glGenBuffers(1, &brick_votes_buffer_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, brick_votes_buffer_);
    std::vector<uint32_t> initial_votes(kHashTableSize, 0);
    glBufferData(GL_SHADER_STORAGE_BUFFER, kHashTableSize * sizeof(uint32_t), initial_votes.data(), GL_DYNAMIC_DRAW);

    // Free List (size = 1 (count) + kMaxBricks (indices))
    glGenBuffers(1, &free_list_buffer_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, free_list_buffer_);
    std::vector<int> initial_free(1 + kMaxBricks);
    initial_free[0] = kMaxBricks; // free_count
    for (int i = 0; i < kMaxBricks; ++i) {
        initial_free[1 + i] = i;
    }
    glBufferData(GL_SHADER_STORAGE_BUFFER, initial_free.size() * sizeof(int), initial_free.data(), GL_DYNAMIC_DRAW);

    // Brick Metadata
    glGenBuffers(1, &brick_metadata_buffer_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, brick_metadata_buffer_);
    std::vector<BrickMetadataGPU> initial_metadata(kHashTableSize);
    for (int i = 0; i < kHashTableSize; ++i) {
        initial_metadata[i].gridPos_poolIdx = glm::ivec4(0, 0, 0, -1);
        initial_metadata[i].timing_padding = glm::vec4(-1000.0f, 0, 0, 0);
    }
    glBufferData(GL_SHADER_STORAGE_BUFFER, kHashTableSize * sizeof(BrickMetadataGPU), initial_metadata.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void VoxelBrickManager::_SetupTextures() {
    int bricks_per_dim = static_cast<int>(std::round(std::pow(kMaxBricks, 1.0 / 3.0)));
    int tex_dim = bricks_per_dim * kBrickSize;

    // 1. Float Pool (for sampling with interpolation)
    glGenTextures(1, &brick_pool_texture_);
    glBindTexture(GL_TEXTURE_3D, brick_pool_texture_);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, tex_dim, tex_dim, tex_dim, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    // 2. Atomic Pool (for accumulation)
    glGenTextures(1, &brick_pool_atomic_texture_);
    glBindTexture(GL_TEXTURE_3D, brick_pool_atomic_texture_);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R32UI, tex_dim, tex_dim, tex_dim, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_3D, 0);
}

void VoxelBrickManager::Update(float delta_time, float current_time) {
    if (!initialized_ || !management_shader_->isValid()) return;

    int bricks_per_dim = static_cast<int>(std::round(std::pow(kMaxBricks, 1.0 / 3.0)));
    int tex_dim = bricks_per_dim * kBrickSize;

    // 1. Perform copy from Atomic to Float texture (using results from PREVIOUS frame splatting)
    if (copy_shader_ && copy_shader_->isValid()) {
        copy_shader_->use();
        glBindImageTexture(0, brick_pool_atomic_texture_, 0, GL_TRUE, 0, GL_READ_ONLY, GL_R32UI);
        glBindImageTexture(1, brick_pool_texture_, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_R32F);

        glDispatchCompute(tex_dim / 8, tex_dim / 8, tex_dim / 8);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    // 2. Clear ATOMIC 3D texture for current frame splatting
    if (clear_shader_ && clear_shader_->isValid()) {
        clear_shader_->use();
        glBindImageTexture(0, brick_pool_atomic_texture_, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_R32UI);

        glDispatchCompute(tex_dim / 8, tex_dim / 8, tex_dim / 8);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
    }

    // 3. Management
    management_shader_->use();
    management_shader_->setFloat("u_time", current_time);
    management_shader_->setUint("u_hashTableSize", kHashTableSize);
    management_shader_->setFloat("u_cooldown", 10.0f); // 10 second cooldown

    BindSSBOs();

    glDispatchCompute((kHashTableSize + 63) / 64, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelBrickManager::BindSSBOs() const {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::VoxelBrickVotes(), brick_votes_buffer_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::VoxelFreeList(), free_list_buffer_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, Constants::SsboBinding::VoxelBrickMetadata(), brick_metadata_buffer_);
}

void VoxelBrickManager::BindSampler(GLuint unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_3D, brick_pool_texture_);
}

} // namespace Boidsish
