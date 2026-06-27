#include "cloud_dynamics_manager.h"
#include "service_locator.h"
#include "constants.h"
#include "shader.h"
#include <algorithm>

namespace Boidsish {

    CloudDynamicsManager::CloudDynamicsManager(ServiceLocator& loc) {}

    CloudDynamicsManager::~CloudDynamicsManager() {
        if (lut_texture_) glDeleteTextures(1, &lut_texture_);
        if (ubo_) glDeleteBuffers(1, &ubo_);
    }

    void CloudDynamicsManager::Initialize() {
        // Create 3D LUT texture
        glGenTextures(1, &lut_texture_);
        glBindTexture(GL_TEXTURE_3D, lut_texture_);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32F, kLutSize, kLutSize, kLutSize, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        // Create UBO
        glGenBuffers(1, &ubo_);
        glBindBuffer(GL_UNIFORM_BUFFER, ubo_);
        glBufferData(GL_UNIFORM_BUFFER, kMaxEffects * sizeof(CloudDynamicsEffectGPU), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        update_shader_ = std::make_unique<ComputeShader>("shaders/helpers/cloud_dynamics_update.comp");
    }

    void CloudDynamicsManager::Update(float deltaTime) {
        for (auto it = effects_.begin(); it != effects_.end(); ) {
            it->stateTimer += deltaTime;

            bool advance = false;
            if (it->state == CloudDynamicsState::Pushing && it->stateTimer >= it->pushingDuration) {
                it->state = CloudDynamicsState::FadingAdvection;
                it->stateTimer = 0.0f;
            } else if (it->state == CloudDynamicsState::FadingAdvection && it->stateTimer >= it->fadingAdvectionDuration) {
                it->state = CloudDynamicsState::FadingDensity;
                it->stateTimer = 0.0f;
            } else if (it->state == CloudDynamicsState::FadingDensity && it->stateTimer >= it->fadingDensityDuration) {
                it->state = CloudDynamicsState::Inactive;
            }

            if (it->state == CloudDynamicsState::Inactive) {
                it = effects_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void CloudDynamicsManager::UpdateUBO() {
        std::vector<CloudDynamicsEffectGPU> gpuEffects;
        for (size_t i = 0; i < std::min(effects_.size(), (size_t)kMaxEffects); ++i) {
            const auto& e = effects_[i];
            CloudDynamicsEffectGPU gpu;
            gpu.position_radius = glm::vec4(e.position, e.radius);

            float stateProgress = 0.0f;
            if (e.state == CloudDynamicsState::Pushing) stateProgress = e.stateTimer / e.pushingDuration;
            else if (e.state == CloudDynamicsState::FadingAdvection) stateProgress = e.stateTimer / e.fadingAdvectionDuration;
            else if (e.state == CloudDynamicsState::FadingDensity) stateProgress = e.stateTimer / e.fadingDensityDuration;

            gpu.params = glm::vec4(e.strength, e.densityOffset, (float)e.type, stateProgress);
            gpu.state_info = glm::vec4((float)e.state, 0.0f, 0.0f, 0.0f);
            gpuEffects.push_back(gpu);
        }

        glBindBuffer(GL_UNIFORM_BUFFER, ubo_);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, gpuEffects.size() * sizeof(CloudDynamicsEffectGPU), gpuEffects.data());
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    void CloudDynamicsManager::BakeLUT(float time) {
        if (!update_shader_ || !update_shader_->isValid()) return;

        UpdateUBO();

        update_shader_->use();
        update_shader_->setInt("uLutSize", kLutSize);
        update_shader_->setInt("uNumEffects", (int)std::min(effects_.size(), (size_t)kMaxEffects));
        update_shader_->setVec3("uWorldMin", world_min_);
        update_shader_->setVec3("uWorldMax", world_max_);
        update_shader_->setFloat("uTime", time);

        glBindBufferBase(GL_UNIFORM_BUFFER, Constants::UboBinding::CloudDynamics(), ubo_);
        glBindImageTexture(0, lut_texture_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

        glDispatchCompute(kLutSize / 8, kLutSize / 8, kLutSize / 8);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    void CloudDynamicsManager::AddEffect(const CloudDynamicsEffect& effect) {
        if (effects_.size() < kMaxEffects) {
            effects_.push_back(effect);
        }
    }

} // namespace Boidsish
