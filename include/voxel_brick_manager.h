#pragma once

#include <glm/glm.hpp>
#include <GL/glew.h>
#include <vector>
#include <memory>
#include "constants.h"
#include <shader.h>

namespace Boidsish {

    struct VoxelBrickMetadata {
        glm::ivec4 grid_pos; // xyz: grid pos, w: pool index
    };

    class VoxelBrickManager {
    public:
        VoxelBrickManager();
        ~VoxelBrickManager();

        void Initialize();
        void Update(float delta_time, float time);
        void BindResources();
        void ClearAccumulation();

        GLuint GetBrickPoolTexture() const { return brick_pool_sampling_tex_; }
        GLuint GetAccumulationTexture() const { return brick_pool_accumulation_tex_; }

    private:
        bool initialized_ = false;

        // Dual 3D textures for the brick pool
        // Accumulation: R32UI (fixed-point density and packed direction)
        // Sampling: RGBA16F (normalized density and direction)
        GLuint brick_pool_accumulation_tex_ = 0;
        GLuint brick_pool_sampling_tex_ = 0;

        // SSBOs
        GLuint metadata_ssbo_ = 0;
        GLuint votes_ssbo_ = 0;
        GLuint free_list_ssbo_ = 0;
        GLuint hash_table_ssbo_ = 0;
        GLuint accumulation_ssbo_ = 0;
        GLuint proposed_pos_ssbo_ = 0;

        // Management Shaders
        std::unique_ptr<ComputeShader> manage_shader_;
        std::unique_ptr<ComputeShader> clear_shader_;
        std::unique_ptr<ComputeShader> copy_shader_;

        void _CreateTextures();
        void _CreateBuffers();
        void _LoadShaders();
    };

} // namespace Boidsish
