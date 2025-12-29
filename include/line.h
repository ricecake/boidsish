#pragma once

#include "shape.h"
#include <glm/glm.hpp>
#include <memory>

class Shader;

namespace Boidsish {

class Line : public Shape {
public:
    Line(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color);
    ~Line();

    void render(Shader& shader, const glm::mat4& model_matrix) const override;
    void render() const override;
    glm::mat4 GetModelMatrix() const override;

    void SetPoints(const glm::vec3& start, const glm::vec3& end);
    void SetVisible(bool visible);
    bool IsVisible() const;

private:
    void setupBuffers() const;

    glm::vec3 start_;
    glm::vec3 end_;
    bool visible_ = true;

    mutable unsigned int vao_ = 0;
    mutable unsigned int vbo_ = 0;
};

} // namespace Boidsish
