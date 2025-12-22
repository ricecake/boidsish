#pragma once

#include "shape.h"
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include "shader.h"

namespace Boidsish {

class AircraftShape : public Shape {
public:
    AircraftShape() {
        // Simple prism shape for the aircraft
        std::vector<float> vertices = {
            // Fuselage
            -0.5f, 0.0f, -1.5f,  0.0f, 0.0f, -1.0f,
             0.5f, 0.0f, -1.5f,  0.0f, 0.0f, -1.0f,
             0.0f, 0.5f,  1.5f,  0.0f, 0.0f,  1.0f,

            -0.5f, 0.0f, -1.5f,  0.0f, 0.0f, -1.0f,
             0.0f,-0.5f,  1.5f,  0.0f, 0.0f,  1.0f,
             0.5f, 0.0f, -1.5f,  0.0f, 0.0f, -1.0f,

            // Wings
            -2.0f, 0.0f,  0.0f,  0.0f, 1.0f,  0.0f,
             2.0f, 0.0f,  0.0f,  0.0f, 1.0f,  0.0f,
             0.0f, 0.0f,  0.5f,  0.0f, 1.0f,  0.0f,

            // Tail
             0.0f, 1.0f,  1.0f,  0.0f, 1.0f,  0.0f,
             0.0f, 0.0f,  1.5f,  0.0f, 0.0f,  1.0f,
             0.0f, 0.0f,  1.0f,  0.0f, 0.0f,  1.0f,
        };

        std::vector<unsigned int> indices;
        for (unsigned int i = 0; i < vertices.size() / 6; ++i) {
            indices.push_back(i);
        }

        glGenVertexArrays(1, &vao_);
        glBindVertexArray(vao_);

        glGenBuffers(1, &vbo_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

        glGenBuffers(1, &ebo_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);

        num_indices_ = indices.size();
    }

    ~AircraftShape() {
        glDeleteVertexArrays(1, &vao_);
        glDeleteBuffers(1, &vbo_);
        glDeleteBuffers(1, &ebo_);
    }

    void render() const override {
        shader->use();
        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x_, y_, z_));
        model *= glm::mat4_cast(rotation_);
        shader->setMat4("model", model);
        shader->setVec4("color", glm::vec4(r_, g_, b_, a_));
        glBindVertexArray(vao_);
        glDrawElements(GL_TRIANGLES, num_indices_, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

private:
    GLuint vao_ = 0, vbo_ = 0, ebo_ = 0;
    int num_indices_ = 0;
};

} // namespace Boidsish
