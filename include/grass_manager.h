#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <memory>
#include <map>
#include <mutex>
#include "constants.h"
#include "biome_properties.h"
#include "render_shader.h"

namespace Boidsish {

    struct GrassProperties {
        glm::vec4 colorTop = glm::vec4(0.2f, 0.5f, 0.1f, 1.0f);
        glm::vec4 colorBottom = glm::vec4(0.05f, 0.2f, 0.02f, 1.0f);
        float height = 1.0f;
        float width = 0.1f;
        float rigidity = 0.5f;
        float heightVariance = 0.3f;
        float widthVariance = 0.1f;
        float density = 1.0f;
        float colorVariability = 0.2f;
        uint32_t biomeMask = 0xFFFFFFFF;
        float windInfluence = 1.0f;
        float padding[2];
    };

    struct GrassType {
        GrassProperties props;
        std::string name;
    };

    class GrassManager {
    public:
        GrassManager();
        ~GrassManager();

        void Initialize();
        void Update(float deltaTime, const class Camera& camera, const class ITerrainGenerator& terrainGen, std::shared_ptr<class TerrainRenderManager> renderManager);
        void Render(const glm::mat4& view, const glm::mat4& projection, bool isShadowPass = false);

        void SetCameraPos(const glm::vec3& pos) { last_camera_pos_ = pos; }

        void AddGrassType(const std::string& name, const GrassProperties& props);

        bool IsEnabled() const { return enabled_; }
        void SetEnabled(bool e) { enabled_ = e; }

    private:
        bool initialized_ = false;
        bool enabled_ = true;

        std::vector<GrassType> grass_types_;

        uint32_t grass_props_ubo_ = 0;
        uint32_t grass_instances_ssbo_ = 0;
        uint32_t grass_indirect_buffer_ = 0;

        std::unique_ptr<class ComputeShader> placement_shader_;
        std::shared_ptr<class Shader> grass_shader_;

        struct GrassInstance {
            glm::vec4 pos_rot; // xyz = world pos, w = rotation
            glm::vec4 scale_seed; // x = height, y = width, z = seed, w = unused
        };

        static constexpr uint32_t kMaxGrassInstances = 1024 * 1024; // 1M blades
        glm::vec3 last_camera_pos_{0.0f};

        void _InitializeResources();
        void _UpdatePlacement(const class Camera& camera, const class ITerrainGenerator& terrainGen, std::shared_ptr<class TerrainRenderManager> renderManager);
    };

}
