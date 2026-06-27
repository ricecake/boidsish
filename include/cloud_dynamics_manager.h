#pragma once

#include <vector>
#include <memory>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "IManager.h"

class ComputeShader;

namespace Boidsish {

    class ServiceLocator;

    enum class CloudDynamicsState {
        Pushing,
        FadingAdvection,
        FadingDensity,
        Inactive
    };

    struct CloudDynamicsEffect {
        glm::vec3 position;
        float radius;
        float strength;
        float densityOffset;
        int type; // 0: Sink (Push away), 1: Source (Flow out)
        CloudDynamicsState state = CloudDynamicsState::Pushing;
        float stateTimer = 0.0f;

        float pushingDuration = 2.0f;
        float fadingAdvectionDuration = 5.0f;
        float fadingDensityDuration = 10.0f;
    };

    // GPU structure for UBO (std140)
    struct CloudDynamicsEffectGPU {
        glm::vec4 position_radius;    // xyz: pos, w: radius
        glm::vec4 params;             // x: strength, y: densityOffset, z: type, w: state (normalized 0-1 within current phase?)
        glm::vec4 state_info;         // x: state enum, y: unused, z: unused, w: unused
    };

    class CloudDynamicsManager : public IManager {
    public:
        CloudDynamicsManager(ServiceLocator& loc);
        ~CloudDynamicsManager();

        void Initialize() override;
        void Update(float deltaTime);
        void BakeLUT(float time);

        void AddEffect(const CloudDynamicsEffect& effect);

        GLuint GetLUT() const { return lut_texture_; }
        glm::vec3 GetWorldMin() const { return world_min_; }
        glm::vec3 GetWorldMax() const { return world_max_; }

    private:
        void UpdateUBO();

        GLuint lut_texture_ = 0;
        GLuint ubo_ = 0;
        std::unique_ptr<::ComputeShader> update_shader_;

        std::vector<CloudDynamicsEffect> effects_;

        glm::vec3 world_min_ = glm::vec3(-10000.0f, 0.0f, -10000.0f);
        glm::vec3 world_max_ = glm::vec3(10000.0f, 2000.0f, 10000.0f);

        static constexpr int kLutSize = 128;
        static constexpr int kMaxEffects = 64;
    };

} // namespace Boidsish
