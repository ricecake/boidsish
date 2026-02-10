#pragma once

#include <vector>
#include <functional>
#include <glm/glm.hpp>

namespace Boidsish {

    struct DualContouringVertex {
        glm::vec3 position;
        glm::vec3 normal;
    };

    struct DualContouringMesh {
        std::vector<DualContouringVertex> vertices;
        std::vector<unsigned int> indices;
    };

    /**
     * @brief Simple Manifold Dual Contouring implementation for procedural meshing.
     */
    class DualContouring {
    public:
        using SDFunction = std::function<float(const glm::vec3&)>;
        using GradFunction = std::function<glm::vec3(const glm::vec3&)>;

        static DualContouringMesh Generate(
            const glm::vec3& min_bound,
            const glm::vec3& max_bound,
            float cell_size,
            SDFunction sdf,
            GradFunction grad = nullptr
        );

    private:
        struct EdgeIntersection {
            glm::vec3 p;
            glm::vec3 n;
        };

        static glm::vec3 SolveQEF(const std::vector<EdgeIntersection>& intersections, const glm::vec3& cell_center);
    };

}
