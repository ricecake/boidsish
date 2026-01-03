#pragma once

#include "fire_effect.h"
#include "shader.h"
#include <memory>
#include <vector>

namespace Boidsish {

// This struct is mirrored in the compute shader.
// It must match the layout and padding there.
struct Emitter {
    glm::vec3 position;
    int style;
    glm::vec3 direction;
    float _padding1;
    glm::vec3 velocity;
    float _padding2;
};

class FireEffectManager {
public:
    FireEffectManager();
    ~FireEffectManager();

    void Update(float delta_time, float time);
    void Render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camera_pos);

    // Add a new fire effect and return a pointer to it
    FireEffect* AddEffect(const glm::vec3& position, FireEffectStyle style,
                          const glm::vec3& direction = glm::vec3(0.0f),
                          const glm::vec3& velocity = glm::vec3(0.0f));

private:
    void _EnsureShaderAndBuffers();

    std::vector<std::unique_ptr<FireEffect>> effects_;

    std::unique_ptr<ComputeShader> compute_shader_;
    std::unique_ptr<Shader> render_shader_;

    GLuint particle_buffer_{0};
    GLuint emitter_buffer_{0};
    GLuint dummy_vao_{0};

    bool initialized_{false};
    float time_{0.0f};

    static const int kMaxParticles = 64000;
};

} // namespace Boidsish