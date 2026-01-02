#include "fire_effect_manager.h"
#include "graphics.h" // For logger
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>

namespace Boidsish {

// This must match the struct in the compute shader
struct Particle {
    glm::vec4 pos;
    glm::vec4 vel;
    int style;
    int emitter_index;
    glm::vec2 _padding;
};

FireEffectManager::FireEffectManager() {}

FireEffectManager::~FireEffectManager() {
    if (particle_buffer_ != 0) {
        glDeleteBuffers(1, &particle_buffer_);
    }
    if (emitter_buffer_ != 0) {
        glDeleteBuffers(1, &emitter_buffer_);
    }
    if (atomic_counter_buffer_ != 0) {
        glDeleteBuffers(1, &atomic_counter_buffer_);
    }
    if (dummy_vao_ != 0) {
        glDeleteVertexArrays(1, &dummy_vao_);
    }
}

void FireEffectManager::_EnsureShaderAndBuffers() {
    if (initialized_) {
        return;
    }

    // Create shaders
    compute_shader_ = std::make_unique<ComputeShader>("shaders/fire.comp");
    render_shader_ = std::make_unique<Shader>("shaders/fire.vert", "shaders/fire.frag");

    // Create buffers
    glGenBuffers(1, &particle_buffer_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, particle_buffer_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, kMaxParticles * sizeof(Particle), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &emitter_buffer_);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, emitter_buffer_);
    // Max of 100 emitters for now, can be resized if needed
    glBufferData(GL_SHADER_STORAGE_BUFFER, 100 * sizeof(Emitter), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &atomic_counter_buffer_);
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomic_counter_buffer_);
    glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);
    GLuint zero = 0;
    glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &zero);


    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

    // A dummy VAO is required by OpenGL 4.2 core profile for drawing arrays.
    glGenVertexArrays(1, &dummy_vao_);

    initialized_ = true;
}

FireEffect* FireEffectManager::AddEffect(const glm::vec3& position, FireEffectStyle style, const glm::vec3& direction, const glm::vec3& velocity) {
    _EnsureShaderAndBuffers();
    effects_.push_back(std::make_unique<FireEffect>(position, style, direction, velocity));
    return effects_.back().get();
}

void FireEffectManager::Update(float delta_time, float time) {
    if (!initialized_ || effects_.empty()) {
        return;
    }

    time_ = time;

    // --- Update Emitters ---
    std::vector<Emitter> emitters;
    for (const auto& effect : effects_) {
        if (effect->IsActive()) {
            emitters.push_back({effect->GetPosition(), (int)effect->GetStyle(), effect->GetDirection(), 0.0f, effect->GetVelocity(), 0.0f});
        }
    }

    if (emitters.empty()) {
        return; // No active emitters, nothing to compute
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, emitter_buffer_);
    glBufferData(GL_SHADER_STORAGE_BUFFER, emitters.size() * sizeof(Emitter), emitters.data(), GL_DYNAMIC_DRAW);

    // --- Dispatch Compute Shader ---
    compute_shader_->use();
    compute_shader_->setFloat("u_delta_time", delta_time);
    compute_shader_->setFloat("u_time", time_);
    compute_shader_->setInt("u_num_emitters", emitters.size());

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particle_buffer_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, emitter_buffer_);
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 2, atomic_counter_buffer_);


    // Dispatch enough groups to cover all particles
    glDispatchCompute((kMaxParticles / 256) + 1, 1, 1);

    // Ensure memory operations are finished before rendering
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 2, 0);
}

void FireEffectManager::Render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camera_pos) {
    if (!initialized_ || effects_.empty()) {
        return;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive blending for fire
    glDepthMask(GL_FALSE); // Disable depth writing

    render_shader_->use();
    render_shader_->setMat4("u_view", view);
    render_shader_->setMat4("u_projection", projection);
    render_shader_->setVec3("u_camera_pos", camera_pos);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particle_buffer_);

    // We don't have a VAO for the particles since we generate them in the shader.
    // We can just draw the number of particles we have.
    // A dummy VAO is required by OpenGL 4.2 core profile.
    glBindVertexArray(dummy_vao_);
    glDrawArrays(GL_POINTS, 0, kMaxParticles);
    glBindVertexArray(0);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);

    glDepthMask(GL_TRUE); // Re-enable depth writing
    glDisable(GL_BLEND);
}

} // namespace Boidsish