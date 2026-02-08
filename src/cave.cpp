#include "cave.h"
#include <shader.h>
#include <limits>
#include <cstddef>
#include <glm/gtc/matrix_transform.hpp>

namespace Boidsish {

    Cave::Cave(const DualContouringMesh& mesh) {
        index_count_ = static_cast<GLsizei>(mesh.indices.size());

        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        glGenBuffers(1, &ebo_);

        glBindVertexArray(vao_);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(DualContouringVertex), mesh.vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(unsigned int), mesh.indices.data(), GL_STATIC_DRAW);

        // Position
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DualContouringVertex), (void*)0);
        glEnableVertexAttribArray(0);
        // Normal
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(DualContouringVertex), (void*)offsetof(DualContouringVertex, normal));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);

        // Compute bounds
        min_ = glm::vec3(std::numeric_limits<float>::max());
        max_ = glm::vec3(std::numeric_limits<float>::lowest());
        for (const auto& v : mesh.vertices) {
            min_ = glm::min(min_, v.position);
            max_ = glm::max(max_, v.position);
        }
    }

    Cave::~Cave() {
        if (vao_) glDeleteVertexArrays(1, &vao_);
        if (vbo_) glDeleteBuffers(1, &vbo_);
        if (ebo_) glDeleteBuffers(1, &ebo_);
    }

    void Cave::render() const {
        if (!shader) return;
        render(*shader, GetModelMatrix());
    }

    void Cave::render(Shader& s, const glm::mat4& model_matrix) const {
        s.use();
        s.setMat4("model", model_matrix);
        s.setVec3("material.albedo", glm::vec3(0.35f, 0.3f, 0.25f));
        s.setFloat("material.roughness", 0.9f);
        s.setFloat("material.metallic", 0.0f);
        s.setBool("use_texture", false);

        glBindVertexArray(vao_);
        glDrawElements(GL_TRIANGLES, index_count_, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    glm::mat4 Cave::GetModelMatrix() const {
        return glm::mat4(1.0f); // Cave is already in world space coordinates from generator
    }

}
