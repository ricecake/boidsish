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
}

void VoxelBrickManager::Initialize() {
    if (initialized_) return;

    _SetupBuffers();
    _SetupTexture();

    management_shader_ = std::make_unique<ComputeShader>("shaders/voxel_manage.comp");
    if (!management_shader_->isValid()) {
        logger::ERROR("Failed to compile voxel management shader");
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
        initial_metadata[i].gridPos = glm::ivec3(0);
        initial_metadata[i].poolIndex = -1;
        initial_metadata[i].lastUsedTime = -1000.0f;
    }
    glBufferData(GL_SHADER_STORAGE_BUFFER, kHashTableSize * sizeof(BrickMetadataGPU), initial_metadata.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void VoxelBrickManager::_SetupTexture() {
    glGenTextures(1, &brick_pool_texture_);
    glBindTexture(GL_TEXTURE_3D, brick_pool_texture_);

    // Calculate dimensions to fit kMaxBricks
    // Assuming kMaxBricks is 4096 and kBrickSize is 8.
    // 16x16x16 bricks = 4096.
    // Each brick is 8x8x8.
    // Total texture size: 128x128x128.

    int bricks_per_dim = static_cast<int>(std::round(std::pow(kMaxBricks, 1.0/3.0)));
    int tex_dim = bricks_per_dim * kBrickSize;

    // Use GL_R32F for density (splatting)
    // Or GL_RGBA32F if we want color/extras.
    // The user said "density of things".
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, tex_dim, tex_dim, tex_dim, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_3D, 0);
}

void VoxelBrickManager::Update(float delta_time, float current_time) {
    if (!initialized_ || !management_shader_->isValid()) return;

    // Clear 3D texture every frame
    float clear_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    glClearTexImage(brick_pool_texture_, 0, GL_RED, GL_FLOAT, clear_color);

    management_shader_->use();
    management_shader_->setFloat("u_time", current_time);
    management_shader_->setUint("u_hashTableSize", kHashTableSize);
    management_shader_->setFloat("u_cooldown", 1.0f); // 1 second cooldown

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
