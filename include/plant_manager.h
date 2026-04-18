#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include "render_shader.h"
#include "constants.h"
#include "biome_properties.h"
#include "grass_manager.h"

namespace Boidsish {

    struct PlantProperties {
        glm::vec4 tubeColor = glm::vec4(0.4f, 0.25f, 0.1f, 1.0f);
        glm::vec4 ballColor = glm::vec4(0.8f, 0.2f, 0.5f, 1.0f);
        glm::vec4 grassColor = glm::vec4(0.3f, 0.8f, 0.2f, 1.0f);
        float tubeRadius = 0.15f;
        float tubeLength = 3.0f;
        float ballRadius = 0.4f;
        float grassDensity = 1.0f;
        float curveStrength = 0.5f;
        float spiralFrequency = 2.0f;
        float zigZagStrength = 0.2f;
        float windInfluence = 1.0f;
        uint32_t enabled = 1;
        float _pad0, _pad1, _pad2;
    };

    struct PlantInstance {
        glm::vec4 pos_rot;   // xyz = world pos, w = rotation
        glm::vec4 seed_params; // x = seed, y = biome, zw = unused
    };

    struct PlantVertex {
        glm::vec4 pos;    // xyz = pos, w = u (around)
        glm::vec4 normal; // xyz = normal, w = v (along)
        glm::vec4 color;  // rgb = color, w = type (0=tube, 1=ball)
    };

    class PlantManager {
    public:
        PlantManager();
        ~PlantManager();

        void Initialize();
        void Update(float deltaTime, float time, const class Camera& camera, const class ITerrainGenerator& terrainGen, std::shared_ptr<class TerrainRenderManager> renderManager, uint32_t grassInstancesSSBO, uint32_t grassIndirectBuffer);

        void Render(const glm::mat4& view, const glm::mat4& projection, const GrassManager::RenderResources& resources, uint32_t temporalUbo, bool isShadowPass = false);

        void SetEnabled(bool e) { enabled_ = e; }
        bool IsEnabled() const { return enabled_; }

        PlantProperties& GetProperties() { return props_; }
        void MarkDirty() { props_dirty_ = true; }

    private:
        bool initialized_ = false;
        bool enabled_ = true;
        bool props_dirty_ = true;

        PlantProperties props_;
        uint32_t plant_props_ubo_ = 0;
        uint32_t plant_instances_ssbo_ = 0;
        uint32_t plant_indirect_buffer_ = 0;
        uint32_t plant_generated_vbo_ = 0;
        uint32_t dummy_vao_ = 0;

        std::unique_ptr<class ComputeShader> placement_shader_;
        std::unique_ptr<class ComputeShader> update_shader_;
        std::shared_ptr<class Shader> plant_shader_;

        static constexpr uint32_t kMaxPlantInstances = 10000;
        static constexpr uint32_t kMaxPlantVertices = kMaxPlantInstances * 1200; // ~11.04M used

        void _InitializeResources();
    };

}
