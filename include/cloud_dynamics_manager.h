#pragma once

#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <GL/glew.h>

namespace Boidsish {

    enum class CloudEffectState {
        Pushing,
        FadingAdvection,
        FadingDensity,
        Dead
    };

    struct CloudDynamicsEffect {
        uint32_t id;
        glm::vec3 position;
        float radius;
        float advectionMagnitude; // Positive = push, Negative = pull
        float densityOffset;

        CloudEffectState state = CloudEffectState::Pushing;
        float stateTimer = 0.0f;

        // Durations for each state
        float pushingDuration = 5.0f;
        float fadingAdvectionDuration = 3.0f;
        float fadingDensityDuration = 10.0f;

        float currentAdvectionScale = 1.0f;
        float currentDensityScale = 1.0f;

        bool isStatic = false; // If true, stays in Pushing state forever
    };

    struct CloudDynamicsEffectGPU {
        glm::vec4 position_radius;    // xyz: pos, w: radius
        glm::vec4 advection_density; // x: advection magnitude, y: density offset, zw: unused
    };

    class CloudDynamicsManager {
    public:
        CloudDynamicsManager();
        ~CloudDynamicsManager();

        void Initialize();
        void Update(float deltaTime);
        void UpdateUBO();
        void BindUBO(GLuint binding_point) const;

        uint32_t AddEffect(const glm::vec3& position, float radius, float advectionMagnitude, float densityOffset);
        void RemoveEffect(uint32_t id);
        void Clear();

        const std::vector<CloudDynamicsEffect>& GetEffects() const { return effects_; }
        size_t GetActiveEffectCount() const { return effects_.size(); }

    private:
        std::vector<CloudDynamicsEffect> effects_;
        uint32_t nextId_ = 1;
        GLuint ubo_ = 0;
        bool initialized_ = false;

        static constexpr size_t kMaxEffects = 64;
    };

} // namespace Boidsish
