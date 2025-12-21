#include "point_cloud.h"

#include "graphics.h"
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <shader.h>

namespace Boidsish {

    std::shared_ptr<Shader> PointCloud::point_cloud_shader_ = nullptr;

    PointCloud::PointCloud(const std::vector<float>& vertexData) :
        vao_(0), vbo_(0), vertex_count_(vertexData.size() / 4) { // 4 floats per vertex (x, y, z, value)
        setupMesh(vertexData);
    }

    PointCloud::~PointCloud() {
        glDeleteVertexArrays(1, &vao_);
        glDeleteBuffers(1, &vbo_);
    }

    void PointCloud::setupMesh(const std::vector<float>& vertexData) {
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);

        glBindVertexArray(vao_);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), &vertexData[0], GL_STATIC_DRAW);

        // Position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        // Value attribute
        glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
    }

    void PointCloud::render() const {
        point_cloud_shader_->use();
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(GetX(), GetY(), GetZ()));
        point_cloud_shader_->setMat4("model", model);

        glBindVertexArray(vao_);
        glDrawArrays(GL_POINTS, 0, vertex_count_);
        glBindVertexArray(0);
    }

} // namespace Boidsish
