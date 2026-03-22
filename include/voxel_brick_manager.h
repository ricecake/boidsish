#pragma once

#include <memory>
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
        GLuint GetBrickPoolAtomicTexture() const { return brick_pool_atomic_texture_; }

    private:
        std::unique_ptr<ComputeShader> clear_shader_;
        struct BrickMetadataGPU {
            glm::ivec4 gridPos_poolIdx;
            glm::vec4  timing_padding;
        };

        void _SetupBuffers();
        void _SetupTextures();

        std::unique_ptr<ComputeShader> management_shader_;
        std::unique_ptr<ComputeShader> copy_shader_;

        GLuint brick_votes_buffer_ = 0;
        GLuint free_list_buffer_ = 0;
        GLuint brick_metadata_buffer_ = 0;
        GLuint brick_pool_texture_ = 0;
        GLuint brick_pool_atomic_texture_ = 0;

        bool initialized_ = false;

        static constexpr int kBrickSize = Constants::Class::VoxelBricks::BrickSize();
        static constexpr int kMaxBricks = Constants::Class::VoxelBricks::MaxBricks();
        static constexpr int kHashTableSize = Constants::Class::VoxelBricks::HashTableSize();
    };

} // namespace Boidsish
