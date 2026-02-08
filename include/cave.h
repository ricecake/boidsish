#pragma once

#include "shape.h"
#include "dual_contouring.h"
#include <GL/glew.h>

namespace Boidsish {

    /**
     * @brief A persistent cave entity mesh.
     */
    class Cave : public Shape {
    public:
        Cave(const DualContouringMesh& mesh);
        virtual ~Cave();

        void render() const override;
        void render(Shader& shader, const glm::mat4& model_matrix) const override;
        glm::mat4 GetModelMatrix() const override;
        std::string GetInstanceKey() const override { return "Cave"; }

        glm::vec3 GetMin() const { return min_; }
        glm::vec3 GetMax() const { return max_; }

    private:
        GLuint vao_ = 0, vbo_ = 0, ebo_ = 0;
        GLsizei index_count_ = 0;
        glm::vec3 min_, max_;
    };

}
