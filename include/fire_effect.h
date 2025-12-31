#pragma once

#include <glm/glm.hpp>
#include <memory>

class Shader;
class ComputeShader;

namespace Boidsish {

class FireEffect {
public:
    FireEffect(
        size_t id, // Add ID to constructor
        const glm::vec3& position,
        size_t particle_count,
        std::shared_ptr<ComputeShader> compute_shader,
        std::shared_ptr<Shader> render_shader
    );
    ~FireEffect();

    FireEffect(const FireEffect&) = delete;
    FireEffect& operator=(const FireEffect&) = delete;

    FireEffect(FireEffect&&) noexcept;
    FireEffect& operator=(FireEffect&&) noexcept;

    void Update(float time, float delta_time);
    void Render(const glm::mat4& view, const glm::mat4& projection);

    void SetPosition(const glm::vec3& position);
    const glm::vec3& GetPosition() const;
    size_t GetId() const;

private:
    void InitializeBuffers();
    void CleanUp();

    size_t m_id;
    glm::vec3 m_position;
    size_t m_particle_count;

    std::shared_ptr<ComputeShader> m_compute_shader;
    std::shared_ptr<Shader> m_render_shader;

    unsigned int m_vao;
    unsigned int m_particle_ssbo;
};

} // namespace Boidsish
