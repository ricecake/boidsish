#pragma once

#include "shape.h"
#include <GL/glew.h>

namespace Boidsish {

class Laser : public Shape {
public:
    Laser();
    ~Laser();

    void SetPoints(const glm::vec3& start, const glm::vec3& end);
    void render(Shader& shader, const glm::mat4& viewProjectionMatrix) const override;
    void render() const override;
    glm::mat4 GetModelMatrix() const override;
private:
    mutable GLuint vao_ = 0, vbo_ = 0;
    glm::vec3 start_point_{0.0f, 0.0f, 0.0f};
    glm::vec3 end_point_{0.0f, 0.0f, 0.0f};

    void SetupBuffers() const;
};

} // namespace Boidsish
