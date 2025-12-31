#pragma once

#include "fire_effect.h"
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <map>

class Shader;
class ComputeShader;

namespace Boidsish {

class FireEffectManager {
public:
    FireEffectManager();
    ~FireEffectManager();

    FireEffectManager(const FireEffectManager&) = delete;
    FireEffectManager& operator=(const FireEffectManager&) = delete;

    size_t AddEffect(const glm::vec3& position, size_t particle_count = 1024);
    void UpdateEffectPosition(size_t id, const glm::vec3& position);

    void Update(float time, float delta_time);
    void Render(const glm::mat4& view, const glm::mat4& projection);

private:
    std::map<size_t, FireEffect> m_effects;
    size_t m_next_id = 0;

    std::shared_ptr<ComputeShader> m_compute_shader;
    std::shared_ptr<Shader> m_render_shader;
};

} // namespace Boidsish
