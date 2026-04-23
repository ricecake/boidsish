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
        glm::vec3 gravity;
        float dt;
        float tau;
        float omega;
        glm::ivec3 resolution;
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
        }
    }

    void FluidLbmManager::Initialize(const FluidLbmConfig& config) {
        config_ = config;
        CreateTextures();
        CreateShaders();

        glGenBuffers(1, &bvhNodesBuffer_);
        glGenBuffers(1, &bvhIndicesBuffer_);
        glGenBuffers(1, &meshVerticesBuffer_);

        // Clear textures
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
        params.gravity = glm::vec3(0.0f, -config_.gravity * 0.1f, 0.0f);
        params.dt = 1.0f;
        params.tau = 0.5f + 3.0f * config_.viscosity;
        params.omega = 1.0f / params.tau;
        params.resolution = config_.resolution;

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

            glUniform3fv(glGetUniformLocation(lbmStepShader_, "u_params.gravity"), 1, glm::value_ptr(params.gravity));
            glUniform1f(glGetUniformLocation(lbmStepShader_, "u_params.dt"), params.dt);
            glUniform1f(glGetUniformLocation(lbmStepShader_, "u_params.tau"), params.tau);
            glUniform1f(glGetUniformLocation(lbmStepShader_, "u_params.omega"), params.omega);
            glUniform3iv(glGetUniformLocation(lbmStepShader_, "u_params.resolution"), 1, glm::value_ptr(params.resolution));

            glDispatchCompute((config_.resolution.x + 3) / 4, (config_.resolution.y + 3) / 4, (config_.resolution.z + 3) / 4);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

            glUseProgram(lbmSurfaceShader_);
            for(int i=0; i<5; ++i) {
                glBindImageTexture(i, outPops[i], 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32F);
            }
            glBindImageTexture(5, massTexture_, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32F);
            glBindImageTexture(6, velocityTexture_, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32F);

            glUniform3iv(glGetUniformLocation(lbmSurfaceShader_, "u_params.resolution"), 1, glm::value_ptr(params.resolution));

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
        glUniform3iv(glGetUniformLocation(lbmRenderShader_, "u_resolution"), 1, glm::value_ptr(config_.resolution));
        glUniform3fv(glGetUniformLocation(lbmRenderShader_, "u_worldScale"), 1, glm::value_ptr(config_.worldScale));
        glUniform3fv(glGetUniformLocation(lbmRenderShader_, "u_worldOrigin"), 1, glm::value_ptr(config_.worldOrigin));
        glUniform3f(glGetUniformLocation(lbmRenderShader_, "u_waterColor"), 0.0f, 0.4f, 0.8f);

        glm::mat4 model = glm::translate(glm::mat4(1.0f), config_.worldOrigin);
        glm::mat4 scale = glm::scale(glm::mat4(1.0f), config_.worldScale);
        glm::mat4 finalModel = model * scale;
        glUniformMatrix4fv(glGetUniformLocation(lbmRenderShader_, "model"), 1, GL_FALSE, glm::value_ptr(finalModel));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_3D, massTexture_);
        glUniform1i(glGetUniformLocation(lbmRenderShader_, "u_mass"), 0);

        if (depthTexture != 0) {
            glActiveTexture(GL_TEXTURE2); // Use unit 2 as per layout(binding = 2) in frag
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
        glUniform1f(glGetUniformLocation(lbmInitShader_, "u_radius"), radius);
        glUniform1f(glGetUniformLocation(lbmInitShader_, "u_amount"), amount);
        glUniform3iv(glGetUniformLocation(lbmInitShader_, "u_resolution"), 1, glm::value_ptr(config_.resolution));
        glUniform3fv(glGetUniformLocation(lbmInitShader_, "u_worldScale"), 1, glm::value_ptr(config_.worldScale));
        glUniform3fv(glGetUniformLocation(lbmInitShader_, "u_worldOrigin"), 1, glm::value_ptr(config_.worldOrigin));
        glUniform1i(glGetUniformLocation(lbmInitShader_, "u_mode"), 0);

        glDispatchCompute((config_.resolution.x + 3) / 4, (config_.resolution.y + 3) / 4, (config_.resolution.z + 3) / 4);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
    }

    void FluidLbmManager::InjectFluidFromModel(std::shared_ptr<Model> model, float amount) {
        if (!initialized_) return;

        auto cached = BuildOrGetBvh(model);

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

        glUniform1f(glGetUniformLocation(lbmInitShader_, "u_amount"), amount);
        glUniform3iv(glGetUniformLocation(lbmInitShader_, "u_resolution"), 1, glm::value_ptr(config_.resolution));
        glUniform3fv(glGetUniformLocation(lbmInitShader_, "u_worldScale"), 1, glm::value_ptr(config_.worldScale));
        glUniform3fv(glGetUniformLocation(lbmInitShader_, "u_worldOrigin"), 1, glm::value_ptr(config_.worldOrigin));
        glUniform1i(glGetUniformLocation(lbmInitShader_, "u_mode"), 1);

        glDispatchCompute((config_.resolution.x + 3) / 4, (config_.resolution.y + 3) / 4, (config_.resolution.z + 3) / 4);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
    }

    void FluidLbmManager::AddObstacleModel(std::shared_ptr<Model> model) {
        obstacleModels_.push_back(model);
    }

    void FluidLbmManager::UpdateObstacles() {
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

        glUniform3iv(glGetUniformLocation(lbmVoxelizeShader_, "u_resolution"), 1, glm::value_ptr(config_.resolution));
        glUniform3fv(glGetUniformLocation(lbmVoxelizeShader_, "u_worldScale"), 1, glm::value_ptr(config_.worldScale));
        glUniform3fv(glGetUniformLocation(lbmVoxelizeShader_, "u_worldOrigin"), 1, glm::value_ptr(config_.worldOrigin));

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
