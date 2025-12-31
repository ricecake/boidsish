#pragma once

#include <glm/glm.hpp>
#include <memory>

class Shader; // Forward declaration
class ComputeShader; // Forward declaration for compute shaders

namespace Boidsish {

class FireEffect {
public:
    FireEffect(
        const glm::vec3& position,
        size_t particle_count,
        std::shared_ptr<ComputeShader> compute_shader, // Changed to ComputeShader
        std::shared_ptr<Shader> render_shader
    );
    ~FireEffect();

    // Non-copyable
    FireEffect(const FireEffect&) = delete;
    FireEffect& operator=(const FireEffect&) = delete;

    // Movable
    FireEffect(FireEffect&&) noexcept;
    FireEffect& operator=(FireEffect&&) noexcept;

    void Update(float time, float delta_time);
    void Render(const glm::mat4& view, const glm::mat4& projection);

    void SetPosition(const glm::vec3& position);
    const glm::vec3& GetPosition() const;

private:
    void InitializeBuffers();
    void CleanUp();

    glm::vec3 m_position;
    size_t m_particle_count;

    std::shared_ptr<ComputeShader> m_compute_shader; // Changed to ComputeShader
    std::shared_ptr<Shader> m_render_shader;

    unsigned int m_vao;
    unsigned int m_particle_ssbo;
};

} // namespace Boidsish
