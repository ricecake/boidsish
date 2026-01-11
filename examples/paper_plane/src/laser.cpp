#include "laser.h"
#include "graphics.h"
#include "shader.h"
#include <GL/glew.h>

namespace Boidsish {

Laser::Laser() {
    // Constructor
}

Laser::~Laser() {
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
        glDeleteBuffers(1, &vbo_);
    }
}

void Laser::SetPoints(const glm::vec3& start, const glm::vec3& end) {
    start_point_ = start;
    end_point_ = end;
    SetupBuffers();
}

void Laser::render(Shader& shader, const glm::mat4& viewProjectionMatrix) const {
    if (vao_ == 0) return;

    shader.use();
    shader.setMat4("model", GetModelMatrix());
    shader.setMat4("viewProjection", viewProjectionMatrix);
    shader.setVec3("objectColor", 1.0f, 0.0f, 0.0f); // Red color

    glBindVertexArray(vao_);
    glDrawArrays(GL_LINES, 0, 2);
    glBindVertexArray(0);
}

void Laser::render() const {
    // Intentionally empty. Laser is rendered via the other render overload.
}

glm::mat4 Laser::GetModelMatrix() const {
    return glm::mat4(1.0f);
}

void Laser::SetupBuffers() const {
    if (vao_ == 0) {
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
    }

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    float vertices[] = {
        start_point_.x, start_point_.y, start_point_.z,
        end_point_.x, end_point_.y, end_point_.z
    };

    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    // Vertex positions
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

} // namespace Boidsish
