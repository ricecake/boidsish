#pragma once

#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "constants.h"
#include "shader.h"

namespace Boidsish {

    class VoxelBrickManager {
    public:
        VoxelBrickManager();
        ~VoxelBrickManager();

        void Initialize();
        void Update(float delta_time, float current_time);

        void BindSSBOs() const;
        void BindSampler(GLuint unit) const;
        GLuint GetBrickPoolTexture() const { return brick_pool_texture_; }

    private:
        struct BrickMetadataGPU {
            glm::ivec3 gridPos;
            int        poolIndex;
            float      lastUsedTime;
            int        padding[3];
        };

        void _SetupBuffers();
        void _SetupTexture();

        std::unique_ptr<ComputeShader> management_shader_;

        GLuint brick_votes_buffer_ = 0;
        GLuint free_list_buffer_ = 0;
        GLuint brick_metadata_buffer_ = 0;
        GLuint brick_pool_texture_ = 0;

        bool initialized_ = false;

        static constexpr int kBrickSize = Constants::Class::VoxelBricks::BrickSize();
        static constexpr int kMaxBricks = Constants::Class::VoxelBricks::MaxBricks();
        static constexpr int kHashTableSize = Constants::Class::VoxelBricks::HashTableSize();
    };

} // namespace Boidsish
