#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <memory>
#include <map>
#include <mutex>
#include <array>
#include "constants.h"
#include "biome_properties.h"
#include "render_shader.h"

namespace Boidsish {

    struct GrassProperties {
        glm::vec4 colorTop = glm::vec4(0.3f, 0.8f, 0.2f, 1.0f);
        glm::vec4 colorBottom = glm::vec4(0.1f, 0.3f, 0.05f, 1.0f);
        float height = 1.0f;
        float width = 0.1f;
        float rigidity = 0.5f;
        float heightVariance = 0.3f;
        float widthVariance = 0.1f;
        float density = 1.0f;
        float colorVariability = 0.2f;
        float windInfluence = 1.0f;
        uint32_t enabled = 0;
        // Pad to 16-byte alignment without using an array, since std140 arrays
        // have 16-byte element stride which would blow up the struct size.
        float _pad0 = 0.0f;
        float _pad1 = 0.0f;
        float _pad2 = 0.0f;
    };

    struct GlobalGrassProperties {
        float lengthMultiplier = 1.0f;
        float widthMultiplier = 1.0f;
        float densityMultiplier = 1.0f;
        float rigidityMultiplier = 1.0f;
        float windMultiplier = 1.0f;
        uint32_t enabled = 1;
        float _pad0 = 0.0f;
        float _pad1 = 0.0f;
    };

    class GrassManager {
    public:
        GrassManager();
        ~GrassManager();

        void Initialize();
        void Update(float deltaTime, float time, const class Camera& camera, const class ITerrainGenerator& terrainGen, std::shared_ptr<class TerrainRenderManager> renderManager);

        struct RenderResources {
            uint32_t lightingUbo;
            uint32_t shadowUbo;
            uint32_t shadowMaps;
            uint32_t transmittanceLUT;
            uint32_t skyViewLUT;
            uint32_t aerialPerspectiveLUT;
            uint32_t cloudShadowMap;
            float atmosphereHeight;
            uint32_t noiseTexture;
            uint32_t curlTexture;
            uint32_t extraNoiseTexture;
            const int* shadowIndices;
        };

        void Render(const glm::mat4& view, const glm::mat4& projection, std::shared_ptr<class TerrainRenderManager> renderManager, const RenderResources& resources, bool isShadowPass = false);

        void SetCameraPos(const glm::vec3& pos) { last_camera_pos_ = pos; }

        void SetGrassProperties(Biome biome, const GrassProperties& props);
        void PopulateDefaultGrassProperties();

        bool IsEnabled() const { return global_props_.enabled != 0; }
        void SetEnabled(bool e) {
            global_props_.enabled = e ? 1 : 0;
            props_dirty_ = true;
        }

        const GlobalGrassProperties& GetGlobalProperties() const { return global_props_; }
        void SetGlobalProperties(const GlobalGrassProperties& props) {
            global_props_ = props;
            props_dirty_ = true;
        }

    private:
        bool initialized_ = false;

        std::array<GrassProperties, 8> biome_grass_props_;
        GlobalGrassProperties global_props_;
        bool props_dirty_ = false;

        uint32_t grass_props_ubo_ = 0;
        uint32_t grass_instances_ssbo_ = 0;
        uint32_t grass_indirect_buffer_ = 0;

        std::unique_ptr<class ComputeShader> placement_shader_;
        std::shared_ptr<class Shader> grass_shader_;

        struct GrassInstance {
            glm::vec4 pos_rot; // xyz = world pos, w = rotation
            glm::vec4 scale_seed_biome; // x = height, y = width, z = seed, w = biome index
        };

        static constexpr uint32_t kMaxGrassInstances = 1024 * 1024; // 1M blades
        glm::vec3 last_camera_pos_{0.0f};
        uint32_t dummy_vao_ = 0;

        void _InitializeResources();
        void _UpdatePlacement(const class Camera& camera, const class ITerrainGenerator& terrainGen, std::shared_ptr<class TerrainRenderManager> renderManager);
    };

}
