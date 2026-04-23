#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <string>
#include <map>
#include "opengl_buffer.h"

namespace Boidsish {

    struct FluidLbmConfig {
        glm::ivec3 resolution = {64, 64, 64};
        float viscosity = 0.01f;
        float gravity = 9.81f;
        glm::vec3 worldScale = {10.0f, 10.0f, 10.0f};
        glm::vec3 worldOrigin = {0.0f, 0.0f, 0.0f};
    };

    class Model;

    class FluidLbmManager {
    public:
        FluidLbmManager();
        ~FluidLbmManager();

        void Initialize(const FluidLbmConfig& config);
        void Step(float dt);
        void Render(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& cameraPos, uint32_t depthTexture = 0);

        void InjectFluid(const glm::vec3& center, float radius, float amount);
        void InjectFluidFromModel(std::shared_ptr<Model> model, float amount);

        // Add a model as an obstacle
        void AddObstacleModel(std::shared_ptr<Model> model);
        void SetTerrainData(uint32_t heightmap, uint32_t chunkGrid, uint32_t dataUbo) {
            terrainHeightmap_ = heightmap;
            terrainChunkGrid_ = chunkGrid;
            terrainDataUbo_ = dataUbo;
        }

        // Accessors
        const FluidLbmConfig& GetConfig() const { return config_; }

    private:
        struct CachedBvh {
            std::vector<uint8_t> nodes;
            std::vector<uint32_t> indices;
            std::vector<glm::vec4> vertices;
            uint32_t numNodes;
        };

        void CreateTextures();
        void CreateShaders();
        void UpdateObstacles();
        CachedBvh BuildOrGetBvh(std::shared_ptr<Model> model);

        FluidLbmConfig config_;
        bool initialized_ = false;

        // GPU Resources
        // We use 5 RGBA32F 3D textures to store 19 populations (double buffered = 10 textures)
        uint32_t populationsA_[5] = {0};
        uint32_t populationsB_[5] = {0};
        bool useA_ = true;

        uint32_t massTexture_ = 0;      // R32F: Mass fraction (0 to 1)
        uint32_t obstacleTexture_ = 0;  // R8: 1 if obstacle, 0 otherwise
        uint32_t velocityTexture_ = 0;  // RGB32F: Macroscopic velocity (cached for rendering)
        uint32_t terrainHeightmap_ = 0;
        uint32_t terrainChunkGrid_ = 0;
        uint32_t terrainDataUbo_ = 0;

        // BVH SSBOs
        uint32_t bvhNodesBuffer_ = 0;
        uint32_t bvhIndicesBuffer_ = 0;
        uint32_t meshVerticesBuffer_ = 0;

        // Shaders
        uint32_t lbmStepShader_ = 0;
        uint32_t lbmSurfaceShader_ = 0;
        uint32_t lbmInitShader_ = 0;
        uint32_t lbmVoxelizeShader_ = 0;
        uint32_t lbmRenderShader_ = 0;

        // Obstacles
        std::vector<std::shared_ptr<Model>> obstacleModels_;
        std::map<std::string, CachedBvh> bvhCache_;
    };

} // namespace Boidsish
