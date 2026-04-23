#include "fluid_lbm_manager.h"
#include <GL/glew.h>
#include "logger.h"
#include "asset_manager.h"
#include "service_locator.h"
#include "shader.h"
#include "model.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define TINYBVH_IMPLEMENTATION
#include "tiny_bvh.h"

namespace Boidsish {

    struct LbmParamsGpu {
        glm::vec4 gravity_dt;      // xyz = gravity, w = dt
        glm::vec4 lbm_amount;      // x = tau, y = omega, z = amount, w = unused
        glm::ivec4 resolution;     // xyz = res, w = unused
        glm::vec4 world_scale_rad; // xyz = scale, w = radius
        glm::vec4 world_origin;    // xyz = origin, w = unused
    };

    FluidLbmManager::FluidLbmManager() {}

    FluidLbmManager::~FluidLbmManager() {
        if (initialized_) {
            glDeleteTextures(5, populationsA_);
            glDeleteTextures(5, populationsB_);
            glDeleteTextures(1, &massTexture_);
            glDeleteTextures(1, &obstacleTexture_);
            glDeleteTextures(1, &velocityTexture_);

            if (lbmStepShader_) glDeleteProgram(lbmStepShader_);
            if (lbmSurfaceShader_) glDeleteProgram(lbmSurfaceShader_);
            if (lbmInitShader_) glDeleteProgram(lbmInitShader_);
            if (lbmVoxelizeShader_) glDeleteProgram(lbmVoxelizeShader_);
            if (lbmRenderShader_) glDeleteProgram(lbmRenderShader_);

            if (bvhNodesBuffer_) glDeleteBuffers(1, &bvhNodesBuffer_);
            if (bvhIndicesBuffer_) glDeleteBuffers(1, &bvhIndicesBuffer_);
            if (meshVerticesBuffer_) glDeleteBuffers(1, &meshVerticesBuffer_);
            if (paramsUbo_) glDeleteBuffers(1, &paramsUbo_);
        }
    }

    void FluidLbmManager::Initialize(const FluidLbmConfig& config) {
        config_ = config;
        CreateTextures();
        CreateShaders();

        glGenBuffers(1, &bvhNodesBuffer_);
        glGenBuffers(1, &bvhIndicesBuffer_);
        glGenBuffers(1, &meshVerticesBuffer_);
        glGenBuffers(1, &paramsUbo_);
        glBindBuffer(GL_UNIFORM_BUFFER, paramsUbo_);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(LbmParamsGpu), nullptr, GL_DYNAMIC_DRAW);

        float zeroF = 0.0f;
        uint32_t zeroU = 0;
        glClearTexImage(massTexture_, 0, GL_RED, GL_FLOAT, &zeroF);
        glClearTexImage(obstacleTexture_, 0, GL_RED, GL_UNSIGNED_BYTE, &zeroU);
        glClearTexImage(velocityTexture_, 0, GL_RGBA, GL_FLOAT, &zeroF);
        for(int i=0; i<5; ++i) {
            glClearTexImage(populationsA_[i], 0, GL_RGBA, GL_FLOAT, &zeroF);
            glClearTexImage(populationsB_[i], 0, GL_RGBA, GL_FLOAT, &zeroF);
        }

        initialized_ = true;
        obstaclesDirty_ = true;
        logger::INFO("FluidLbmManager initialized with resolution {}x{}x{}", config_.resolution.x, config_.resolution.y, config_.resolution.z);
    }

    void FluidLbmManager::CreateTextures() {
        auto create3DTexture = [&](GLuint& tex, GLenum internalFormat, GLenum format, GLenum type) {
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_3D, tex);
            glTexImage3D(GL_TEXTURE_3D, 0, internalFormat, config_.resolution.x, config_.resolution.y, config_.resolution.z, 0, format, type, nullptr);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        };

        for (int i = 0; i < 5; ++i) {
            create3DTexture(populationsA_[i], GL_RGBA32F, GL_RGBA, GL_FLOAT);
            create3DTexture(populationsB_[i], GL_RGBA32F, GL_RGBA, GL_FLOAT);
        }

        create3DTexture(massTexture_, GL_R32F, GL_RED, GL_FLOAT);
        create3DTexture(obstacleTexture_, GL_R8, GL_RED, GL_UNSIGNED_BYTE);
        create3DTexture(velocityTexture_, GL_RGBA32F, GL_RGBA, GL_FLOAT);
    }

    void FluidLbmManager::CreateShaders() {
        ComputeShader step("shaders/lbm_step.comp");
        if (step.isValid()) lbmStepShader_ = step.ID; step.ID = 0;

        ComputeShader surface("shaders/lbm_surface.comp");
        if (surface.isValid()) lbmSurfaceShader_ = surface.ID; surface.ID = 0;

        ComputeShader init("shaders/lbm_init.comp");
        if (init.isValid()) lbmInitShader_ = init.ID; init.ID = 0;

        ComputeShader vox("shaders/lbm_voxelize.comp");
        if (vox.isValid()) lbmVoxelizeShader_ = vox.ID; vox.ID = 0;

        Shader render("shaders/lbm_render.vert", "shaders/lbm_render.frag");
        if (render.isValid()) lbmRenderShader_ = render.ID; render.ID = 0;
    }

    void FluidLbmManager::Step(float dt) {
        if (!initialized_) return;

        UpdateObstacles();

        LbmParamsGpu params;
        params.gravity_dt = glm::vec4(0.0f, -config_.gravity * 0.001f, 0.0f, 1.0f); // Slower gravity in lattice units
        params.lbm_amount.x = 0.5f + 3.0f * config_.viscosity; // tau
        params.lbm_amount.y = 1.0f / params.lbm_amount.x; // omega
        params.resolution = glm::ivec4(config_.resolution, 0);
        params.world_scale_rad = glm::vec4(config_.worldScale, 0.0f);
        params.world_origin = glm::vec4(config_.worldOrigin, 0.0f);

        glBindBuffer(GL_UNIFORM_BUFFER, paramsUbo_);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(LbmParamsGpu), &params);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, paramsUbo_);

        int subSteps = 4;
        for (int s = 0; s < subSteps; ++s) {
            GLuint inPops[5], outPops[5];
            if (useA_) {
                for(int i=0; i<5; ++i) { inPops[i] = populationsA_[i]; outPops[i] = populationsB_[i]; }
            } else {
                for(int i=0; i<5; ++i) { inPops[i] = populationsB_[i]; outPops[i] = populationsA_[i]; }
            }

            glUseProgram(lbmStepShader_);
            for(int i=0; i<5; ++i) {
                glBindImageTexture(i, inPops[i], 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32F);
                glBindImageTexture(5 + i, outPops[i], 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);
            }
            glBindImageTexture(10, massTexture_, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32F);
            glBindImageTexture(11, obstacleTexture_, 0, GL_TRUE, 0, GL_READ_ONLY, GL_R8);
            glBindImageTexture(12, velocityTexture_, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);

            glDispatchCompute((config_.resolution.x + 3) / 4, (config_.resolution.y + 3) / 4, (config_.resolution.z + 3) / 4);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

            glUseProgram(lbmSurfaceShader_);
            for(int i=0; i<5; ++i) {
                glBindImageTexture(i, outPops[i], 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32F);
            }
            glBindImageTexture(5, massTexture_, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32F);
            glBindImageTexture(6, velocityTexture_, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32F);

            glDispatchCompute((config_.resolution.x + 3) / 4, (config_.resolution.y + 3) / 4, (config_.resolution.z + 3) / 4);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

            useA_ = !useA_;
        }
    }

    void FluidLbmManager::Render(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& cameraPos, uint32_t depthTexture) {
        if (!initialized_) return;

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glUseProgram(lbmRenderShader_);

        glUniformMatrix4fv(glGetUniformLocation(lbmRenderShader_, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(lbmRenderShader_, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
        glUniform3fv(glGetUniformLocation(lbmRenderShader_, "u_cameraPos"), 1, glm::value_ptr(cameraPos));
        glUniform3f(glGetUniformLocation(lbmRenderShader_, "u_waterColor"), 0.1f, 0.4f, 0.7f);

        glBindBufferBase(GL_UNIFORM_BUFFER, 0, paramsUbo_);

        glm::mat4 model = glm::translate(glm::mat4(1.0f), config_.worldOrigin);
        glm::mat4 scale = glm::scale(glm::mat4(1.0f), config_.worldScale);
        glm::mat4 finalModel = model * scale;
        glUniformMatrix4fv(glGetUniformLocation(lbmRenderShader_, "model"), 1, GL_FALSE, glm::value_ptr(finalModel));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_3D, massTexture_);
        glUniform1i(glGetUniformLocation(lbmRenderShader_, "u_mass"), 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_3D, velocityTexture_);
        glUniform1i(glGetUniformLocation(lbmRenderShader_, "u_velocity"), 1);

        if (depthTexture != 0) {
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, depthTexture);
            glUniform1i(glGetUniformLocation(lbmRenderShader_, "u_depthTexture"), 2);
        }

        static GLuint vao = 0, vbo = 0, ebo = 0;
        if (vao == 0) {
            float vertices[] = {
                0,0,0, 1,0,0, 1,1,0, 0,1,0, 0,0,1, 1,0,1, 1,1,1, 0,1,1
            };
            uint32_t indices[] = {
                0,1,2, 0,2,3, 4,5,6, 4,6,7, 0,4,7, 0,7,3, 1,5,6, 1,6,2, 0,1,5, 0,5,4, 3,2,6, 3,6,7
            };
            glGenVertexArrays(1, &vao);
            glGenBuffers(1, &vbo);
            glGenBuffers(1, &ebo);
            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        }
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        glDisable(GL_BLEND);
    }

    void FluidLbmManager::InjectFluid(const glm::vec3& center, float radius, float amount) {
        if (!initialized_) return;

        LbmParamsGpu params;
        params.lbm_amount.z = amount;
        params.world_scale_rad.w = radius;
        params.world_origin = glm::vec4(config_.worldOrigin, 0.0f);
        params.world_scale_rad.x = config_.worldScale.x;
        params.world_scale_rad.y = config_.worldScale.y;
        params.world_scale_rad.z = config_.worldScale.z;
        params.resolution = glm::ivec4(config_.resolution, 0);

        glBindBuffer(GL_UNIFORM_BUFFER, paramsUbo_);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(LbmParamsGpu), &params);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, paramsUbo_);

        glUseProgram(lbmInitShader_);

        GLuint pops[5];
        if (useA_) { for(int i=0; i<5; ++i) pops[i] = populationsA_[i]; }
        else { for(int i=0; i<5; ++i) pops[i] = populationsB_[i]; }

        for(int i=0; i<5; ++i) {
            glBindImageTexture(i, pops[i], 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        }
        glBindImageTexture(5, massTexture_, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_R32F);
        glBindImageTexture(6, velocityTexture_, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);

        glUniform3fv(glGetUniformLocation(lbmInitShader_, "u_center"), 1, glm::value_ptr(center));
        glUniform1i(glGetUniformLocation(lbmInitShader_, "u_mode"), 0);

        glDispatchCompute((config_.resolution.x + 3) / 4, (config_.resolution.y + 3) / 4, (config_.resolution.z + 3) / 4);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
    }

    void FluidLbmManager::InjectFluidFromModel(std::shared_ptr<Model> model, float amount) {
        if (!initialized_) return;

        auto cached = BuildOrGetBvh(model);

        LbmParamsGpu params;
        params.lbm_amount.z = amount;
        params.world_origin = glm::vec4(config_.worldOrigin, 0.0f);
        params.world_scale_rad.x = config_.worldScale.x;
        params.world_scale_rad.y = config_.worldScale.y;
        params.world_scale_rad.z = config_.worldScale.z;
        params.resolution = glm::ivec4(config_.resolution, 0);

        glBindBuffer(GL_UNIFORM_BUFFER, paramsUbo_);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(LbmParamsGpu), &params);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, paramsUbo_);

        glUseProgram(lbmInitShader_);

        GLuint pops[5];
        if (useA_) { for(int i=0; i<5; ++i) pops[i] = populationsA_[i]; }
        else { for(int i=0; i<5; ++i) pops[i] = populationsB_[i]; }

        for(int i=0; i<5; ++i) {
            glBindImageTexture(i, pops[i], 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        }
        glBindImageTexture(5, massTexture_, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_R32F);
        glBindImageTexture(6, velocityTexture_, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, bvhNodesBuffer_);
        glBufferData(GL_SHADER_STORAGE_BUFFER, cached.nodes.size(), cached.nodes.data(), GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bvhNodesBuffer_);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, bvhIndicesBuffer_);
        glBufferData(GL_SHADER_STORAGE_BUFFER, cached.indices.size() * sizeof(uint32_t), cached.indices.data(), GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, bvhIndicesBuffer_);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, meshVerticesBuffer_);
        glBufferData(GL_SHADER_STORAGE_BUFFER, cached.vertices.size() * sizeof(glm::vec4), cached.vertices.data(), GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, meshVerticesBuffer_);

        glm::mat4 invMeshTransform = glm::inverse(model->GetModelMatrix());
        glUniformMatrix4fv(glGetUniformLocation(lbmInitShader_, "u_invMeshTransform"), 1, GL_FALSE, glm::value_ptr(invMeshTransform));
        glUniform1i(glGetUniformLocation(lbmInitShader_, "u_numNodes"), (int)cached.numNodes);
        glUniform1i(glGetUniformLocation(lbmInitShader_, "u_mode"), 1);

        glDispatchCompute((config_.resolution.x + 3) / 4, (config_.resolution.y + 3) / 4, (config_.resolution.z + 3) / 4);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
    }

    void FluidLbmManager::AddObstacleModel(std::shared_ptr<Model> model) {
        obstacleModels_.push_back(model);
        obstaclesDirty_ = true;
    }

    void FluidLbmManager::UpdateObstacles() {
        if (!obstaclesDirty_) return;

        float zero = 0.0f;
        glClearTexImage(obstacleTexture_, 0, GL_RED, GL_FLOAT, &zero);

        glUseProgram(lbmVoxelizeShader_);
        glBindImageTexture(0, obstacleTexture_, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R8);

        if (terrainHeightmap_ != 0) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D_ARRAY, terrainHeightmap_);
            glUniform1i(glGetUniformLocation(lbmVoxelizeShader_, "u_terrainHeightmap"), 1);

            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, terrainChunkGrid_);
            glUniform1i(glGetUniformLocation(lbmVoxelizeShader_, "u_chunkGrid"), 2);

            glBindBufferBase(GL_UNIFORM_BUFFER, 8, terrainDataUbo_);
        }

        glBindBufferBase(GL_UNIFORM_BUFFER, 0, paramsUbo_);

        if (obstacleModels_.empty()) {
            glUniform1i(glGetUniformLocation(lbmVoxelizeShader_, "u_numNodes"), 0);
            glUniform1i(glGetUniformLocation(lbmVoxelizeShader_, "u_firstPass"), 1);
            glDispatchCompute((config_.resolution.x + 3) / 4, (config_.resolution.y + 3) / 4, (config_.resolution.z + 3) / 4);
        } else {
            bool first = true;
            for (const auto& model : obstacleModels_) {
                auto cached = BuildOrGetBvh(model);

                glBindBuffer(GL_SHADER_STORAGE_BUFFER, bvhNodesBuffer_);
                glBufferData(GL_SHADER_STORAGE_BUFFER, cached.nodes.size(), cached.nodes.data(), GL_DYNAMIC_DRAW);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bvhNodesBuffer_);

                glBindBuffer(GL_SHADER_STORAGE_BUFFER, bvhIndicesBuffer_);
                glBufferData(GL_SHADER_STORAGE_BUFFER, cached.indices.size() * sizeof(uint32_t), cached.indices.data(), GL_DYNAMIC_DRAW);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, bvhIndicesBuffer_);

                glBindBuffer(GL_SHADER_STORAGE_BUFFER, meshVerticesBuffer_);
                glBufferData(GL_SHADER_STORAGE_BUFFER, cached.vertices.size() * sizeof(glm::vec4), cached.vertices.data(), GL_DYNAMIC_DRAW);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, meshVerticesBuffer_);

                glm::mat4 invMeshTransform = glm::inverse(model->GetModelMatrix());
                glUniformMatrix4fv(glGetUniformLocation(lbmVoxelizeShader_, "u_invMeshTransform"), 1, GL_FALSE, glm::value_ptr(invMeshTransform));
                glUniform1i(glGetUniformLocation(lbmVoxelizeShader_, "u_numNodes"), (int)cached.numNodes);
                glUniform1i(glGetUniformLocation(lbmVoxelizeShader_, "u_firstPass"), first ? 1 : 0);

                glDispatchCompute((config_.resolution.x + 3) / 4, (config_.resolution.y + 3) / 4, (config_.resolution.z + 3) / 4);
                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
                first = false;
            }
        }
        obstaclesDirty_ = false;
    }

    FluidLbmManager::CachedBvh FluidLbmManager::BuildOrGetBvh(std::shared_ptr<Model> model) {
        std::string key = model->GetInstanceKey();
        if (bvhCache_.count(key)) return bvhCache_[key];

        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        model->GetGeometry(vertices, indices);

        CachedBvh cached;
        if (vertices.empty()) {
            cached.numNodes = 0;
            return cached;
        }

        std::vector<tinybvh::bvhvec4> bvhVerts;
        for (unsigned int idx : indices) {
            const auto& v = vertices[idx];
            bvhVerts.emplace_back(v.Position.x, v.Position.y, v.Position.z, 1.0f);
        }

        tinybvh::BVH bvh;
        bvh.Build(bvhVerts.data(), (uint32_t)bvhVerts.size() / 3);

        cached.numNodes = bvh.usedNodes;
        cached.nodes.resize(bvh.usedNodes * sizeof(tinybvh::BVH::BVHNode));
        memcpy(cached.nodes.data(), bvh.bvhNode, cached.nodes.size());

        cached.indices.assign(bvh.primIdx, bvh.primIdx + bvh.idxCount);
        for(const auto& v : bvhVerts) cached.vertices.push_back(glm::vec4(v.x, v.y, v.z, v.w));

        bvhCache_[key] = cached;
        return cached;
    }

} // namespace Boidsish
