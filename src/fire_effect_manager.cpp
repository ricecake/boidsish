#include "fire_effect_manager.h"
#include "shader.h"

namespace Boidsish {

FireEffectManager::FireEffectManager() {
    m_compute_shader = std::make_shared<ComputeShader>("shaders/fire.comp");
    m_render_shader = std::make_shared<Shader>("shaders/fire.vert", "shaders/fire.frag");
}

FireEffectManager::~FireEffectManager() = default;

size_t FireEffectManager::AddEffect(const glm::vec3& position, size_t particle_count) {
    size_t id = m_next_id++;
    m_effects.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(id),
        std::forward_as_tuple(id, position, particle_count, m_compute_shader, m_render_shader)
    );
    return id;
}

void FireEffectManager::UpdateEffectPosition(size_t id, const glm::vec3& position) {
    auto it = m_effects.find(id);
    if (it != m_effects.end()) {
        it->second.SetPosition(position);
    }
}

void FireEffectManager::Update(float time, float delta_time) {
    for (auto& pair : m_effects) {
        pair.second.Update(time, delta_time);
    }
}

void FireEffectManager::Render(const glm::mat4& view, const glm::mat4& projection) {
    for (auto& pair : m_effects) {
        pair.second.Render(view, projection);
    }
}

} // namespace Boidsish
