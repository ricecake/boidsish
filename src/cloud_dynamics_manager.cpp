#include "cloud_dynamics_manager.h"
#include <algorithm>
#include <cstring>

namespace Boidsish {

    CloudDynamicsManager::CloudDynamicsManager() {}

    CloudDynamicsManager::~CloudDynamicsManager() {
        if (ubo_ != 0) {
            glDeleteBuffers(1, &ubo_);
        }
    }

    void CloudDynamicsManager::Initialize() {
        if (initialized_) return;

        glGenBuffers(1, &ubo_);
        glBindBuffer(GL_UNIFORM_BUFFER, ubo_);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(uint32_t) * 4 + sizeof(CloudDynamicsEffectGPU) * kMaxEffects, nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        initialized_ = true;
    }

    void CloudDynamicsManager::Update(float deltaTime) {
        for (auto it = effects_.begin(); it != effects_.end(); ) {
            if (it->isStatic) {
                ++it;
                continue;
            }

            it->stateTimer += deltaTime;

            switch (it->state) {
                case CloudEffectState::Pushing:
                    if (it->stateTimer >= it->pushingDuration) {
                        it->state = CloudEffectState::FadingAdvection;
                        it->stateTimer = 0.0f;
                    }
                    break;
                case CloudEffectState::FadingAdvection:
                    it->currentAdvectionScale = 1.0f - std::clamp(it->stateTimer / it->fadingAdvectionDuration, 0.0f, 1.0f);
                    if (it->stateTimer >= it->fadingAdvectionDuration) {
                        it->state = CloudEffectState::FadingDensity;
                        it->stateTimer = 0.0f;
                        it->currentAdvectionScale = 0.0f;
                    }
                    break;
                case CloudEffectState::FadingDensity:
                    it->currentDensityScale = 1.0f - std::clamp(it->stateTimer / it->fadingDensityDuration, 0.0f, 1.0f);
                    if (it->stateTimer >= it->fadingDensityDuration) {
                        it->state = CloudEffectState::Dead;
                        it->currentDensityScale = 0.0f;
                    }
                    break;
                default:
                    break;
            }

            if (it->state == CloudEffectState::Dead) {
                it = effects_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void CloudDynamicsManager::UpdateUBO() {
        if (!initialized_) return;

        std::vector<CloudDynamicsEffectGPU> gpuEffects;
        gpuEffects.reserve(std::min(effects_.size(), kMaxEffects));

        for (size_t i = 0; i < std::min(effects_.size(), kMaxEffects); ++i) {
            CloudDynamicsEffectGPU gpuEffect;
            gpuEffect.position_radius = glm::vec4(effects_[i].position, effects_[i].radius);
            gpuEffect.advection_density = glm::vec4(
                effects_[i].advectionMagnitude * effects_[i].currentAdvectionScale,
                effects_[i].densityOffset * effects_[i].currentDensityScale,
                0.0f, 0.0f
            );
            gpuEffects.push_back(gpuEffect);
        }

        uint32_t count = static_cast<uint32_t>(gpuEffects.size());

        glBindBuffer(GL_UNIFORM_BUFFER, ubo_);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(uint32_t), &count);
        if (count > 0) {
            glBufferSubData(GL_UNIFORM_BUFFER, 16, sizeof(CloudDynamicsEffectGPU) * count, gpuEffects.data());
        }
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    void CloudDynamicsManager::BindUBO(GLuint binding_point) const {
        if (!initialized_) return;
        glBindBufferBase(GL_UNIFORM_BUFFER, binding_point, ubo_);
    }

    uint32_t CloudDynamicsManager::AddEffect(const glm::vec3& position, float radius, float advectionMagnitude, float densityOffset) {
        CloudDynamicsEffect effect;
        effect.id = nextId_++;
        effect.position = position;
        effect.radius = radius;
        effect.advectionMagnitude = advectionMagnitude;
        effect.densityOffset = densityOffset;

        effects_.push_back(effect);
        return effect.id;
    }

    void CloudDynamicsManager::RemoveEffect(uint32_t id) {
        effects_.erase(
            std::remove_if(effects_.begin(), effects_.end(), [id](const CloudDynamicsEffect& e) { return e.id == id; }),
            effects_.end()
        );
    }

    void CloudDynamicsManager::Clear() {
        effects_.clear();
    }

} // namespace Boidsish
