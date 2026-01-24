#pragma once

#include <vector>

#include "field.h"
#include <glm/glm.hpp>

namespace Boidsish {

    class Terrain {
    public:
        Terrain(
            const std::vector<unsigned int>& indices,
            const std::vector<glm::vec3>&    vertices,
            const std::vector<glm::vec3>&    normals,
            const PatchProxy&                proxy,
            const glm::vec3&                 position,
            int                              chunk_x,
            int                              chunk_z
        );
        ~Terrain();

        // Public members for field calculations and rendering data access
        PatchProxy                      proxy;
        std::vector<glm::vec3>          vertices;
        std::vector<glm::vec3>          normals;
        std::vector<unsigned int>       indices;
        glm::vec3                       position;
        int                             chunk_x;
        int                             chunk_z;


        float GetX() const { return position.x; }
        float GetY() const { return position.y; }
        float GetZ() const { return position.z; }
    };

} // namespace Boidsish
