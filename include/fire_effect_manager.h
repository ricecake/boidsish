#pragma once

#include "fire_effect.h"
#include <glm/glm.hpp>
#include <memory>
#include <vector>

class Shader;
class ComputeShader; // Forward declare ComputeShader

namespace Boidsish {

class FireEffectManager {
public:
    FireEffectManager();
    ~FireEffectManager();

    // Non-copyable
    FireEffectManager(const FireEffectManager&) = delete;
    FireEffectManager& operator=(const FireEffectManager&) = delete;

    void AddEffect(const glm::vec3& position, size_t particle_count = 1024);

    void Update(float time, float delta_time);
    void Render(const glm::mat4& view, const glm::mat4& projection);

private:
    std::vector<FireEffect> m_effects;

    // Correctly typed compute shader pointer
    std::shared_ptr<ComputeShader> m_compute_shader;
    std::shared_ptr<Shader> m_render_shader;
};

} // namespace Boidsish
