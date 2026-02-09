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

        // Compute bounds and center
        min_ = glm::vec3(std::numeric_limits<float>::max());
        max_ = glm::vec3(std::numeric_limits<float>::lowest());
        for (const auto& v : mesh.vertices) {
            min_ = glm::min(min_, v.position);
            max_ = glm::max(max_, v.position);
        }
        center_ = (min_ + max_) * 0.5f;
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

        // Use terrain-style rock coloring based on depth
        // Deeper caves are darker, surface caves show more varied rock
        float depth_factor = glm::clamp(-center_.y / 50.0f, 0.0f, 1.0f);
        glm::vec3 surface_rock(0.45f, 0.40f, 0.38f);  // Lighter surface rock
        glm::vec3 deep_rock(0.25f, 0.22f, 0.20f);     // Darker deep rock
        glm::vec3 cave_color = glm::mix(surface_rock, deep_rock, depth_factor);

        s.setVec3("objectColor", cave_color);
        s.setFloat("roughness", 0.85f);
        s.setFloat("metallic", 0.0f);
        s.setBool("usePBR", true);
        s.setBool("use_texture", false);
        s.setBool("isColossal", false);
        s.setBool("isCave", true);  // Signal to shader this is a cave

        // Disable backface culling to render both sides of cave surfaces
        glDisable(GL_CULL_FACE);

        glBindVertexArray(vao_);
        glDrawElements(GL_TRIANGLES, index_count_, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        // Re-enable backface culling
        glEnable(GL_CULL_FACE);
    }

    glm::mat4 Cave::GetModelMatrix() const {
        return glm::mat4(1.0f);
    }

}
