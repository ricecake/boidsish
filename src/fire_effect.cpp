#include "fire_effect.h"
#include <GL/glew.h>
#include "shader.h"
#include <vector>

namespace Boidsish {

// Helper struct to match layout in compute shader
struct Particle {
    glm::vec4 position; // w is lifetime
    glm::vec4 velocity;
};

FireEffect::FireEffect(
    size_t id,
    const glm::vec3& position,
    size_t particle_count,
    std::shared_ptr<ComputeShader> compute_shader,
    std::shared_ptr<Shader> render_shader)
    : m_id(id),
      m_position(position),
      m_particle_count(particle_count),
      m_compute_shader(std::move(compute_shader)),
      m_render_shader(std::move(render_shader)),
      m_vao(0),
      m_particle_ssbo(0) {
    InitializeBuffers();
}

FireEffect::~FireEffect() {
    CleanUp();
}

FireEffect::FireEffect(FireEffect&& other) noexcept
    : m_id(other.m_id),
      m_position(other.m_position),
      m_particle_count(other.m_particle_count),
      m_compute_shader(std::move(other.m_compute_shader)),
      m_render_shader(std::move(other.m_render_shader)),
      m_vao(other.m_vao),
      m_particle_ssbo(other.m_particle_ssbo) {
    // Invalidate other's handles so its destructor does nothing
    other.m_id = 0;
    other.m_vao = 0;
    other.m_particle_ssbo = 0;
}

FireEffect& FireEffect::operator=(FireEffect&& other) noexcept {
    if (this != &other) {
        // Clean up our own resources first
        CleanUp();

        // Move resources from other
        m_id = other.m_id;
        m_position = other.m_position;
        m_particle_count = other.m_particle_count;
        m_compute_shader = std::move(other.m_compute_shader);
        m_render_shader = std::move(other.m_render_shader);
        m_vao = other.m_vao;
        m_particle_ssbo = other.m_particle_ssbo;

        // Invalidate other's handles
        other.m_id = 0;
        other.m_vao = 0;
        other.m_particle_ssbo = 0;
    }
    return *this;
}

void FireEffect::InitializeBuffers() {
    // Create VAO
    glGenVertexArrays(1, &m_vao);

    // Create SSBO
    glGenBuffers(1, &m_particle_ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_particle_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, m_particle_count * sizeof(Particle), nullptr, GL_DYNAMIC_DRAW);

    // Initialize particles (all dead initially)
    std::vector<Particle> initial_particles(m_particle_count);
    for (auto& p : initial_particles) {
        p.position = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f); // lifetime = 0
        p.velocity = glm::vec4(0.0f);
    }
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, m_particle_count * sizeof(Particle), initial_particles.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void FireEffect::CleanUp() {
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    if (m_particle_ssbo != 0) {
        glDeleteBuffers(1, &m_particle_ssbo);
        m_particle_ssbo = 0;
    }
}

void FireEffect::Update(float time, float delta_time) {
    m_compute_shader->use();
    m_compute_shader->setFloat("u_time", time);
    m_compute_shader->setFloat("u_delta_time", delta_time);
    m_compute_shader->setVec3("u_emitter_pos", m_position);
    m_compute_shader->setInt("u_particle_count", m_particle_count); // Pass particle count

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_particle_ssbo);

    // Correctly calculate workgroup count, rounding up
    const unsigned int workgroup_size = 256;
    m_compute_shader->dispatch((m_particle_count + workgroup_size - 1) / workgroup_size, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0); // Unbind
}

void FireEffect::Render(const glm::mat4& view, const glm::mat4& projection) {
    glEnable(GL_PROGRAM_POINT_SIZE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive blending for fire

    m_render_shader->use();
    m_render_shader->setMat4("u_view", view);
    m_render_shader->setMat4("u_projection", projection);

    glBindVertexArray(m_vao);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_particle_ssbo);

    glDrawArrays(GL_POINTS, 0, m_particle_count);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0); // Unbind
    glBindVertexArray(0);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Reset blend mode
    glDisable(GL_PROGRAM_POINT_SIZE);
}

void FireEffect::SetPosition(const glm::vec3& position) {
    m_position = position;
}

const glm::vec3& FireEffect::GetPosition() const {
    return m_position;
}

size_t FireEffect::GetId() const {
    return m_id;
}

} // namespace Boidsish
