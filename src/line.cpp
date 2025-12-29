#include "line.h"
#include "shader.h"
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>

namespace Boidsish {

Line::Line(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color)
    : Shape(0, 0, 0, 0, color.r, color.g, color.b), start_(start), end_(end) {}

Line::~Line() {
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
    }
    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
    }
}

void Line::SetPoints(const glm::vec3& start, const glm::vec3& end) {
    start_ = start;
    end_ = end;

    if (vbo_ != 0) {
        float vertices[] = {
            start_.x, start_.y, start_.z,
            end_.x, end_.y, end_.z
        };
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

void Line::SetVisible(bool visible) {
    visible_ = visible;
}

bool Line::IsVisible() const {
    return visible_;
}

void Line::setupBuffers() const {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);

    float vertices[] = {
        start_.x, start_.y, start_.z,
        end_.x, end_.y, end_.z
    };

    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Line::render(Shader& shader, const glm::mat4& model_matrix) const {
    if (!visible_) return;

    if (vao_ == 0) {
        setupBuffers();
    }

    shader.use();
    shader.setMat4("model", model_matrix);
    shader.setVec3("objectColor", GetR(), GetG(), GetB());

    glBindVertexArray(vao_);
    glDrawArrays(GL_LINES, 0, 2);
    glBindVertexArray(0);
}

void Line::render() const {
    if (shader) {
        render(*shader, GetModelMatrix());
    }
}

glm::mat4 Line::GetModelMatrix() const {
    return glm::mat4(1.0f); // The line is already in world coordinates
}

} // namespace Boidsish
