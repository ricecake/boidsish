#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <GL/glew.h>

namespace Boidsish {

    struct TerrainOpening {
        glm::vec3 center;
        float radius;

        TerrainOpening(const glm::vec3& c, float r) : center(c), radius(r) {}
    };

    /**
     * @brief Manages terrain holes/openings via a UBO.
     */
    class TerrainOpeningManager {
    public:
        TerrainOpeningManager();
        ~TerrainOpeningManager();

        void Initialize();
        int AddOpening(const TerrainOpening& opening);
        void RemoveOpening(int id);
        void Clear();

        void UpdateUBO();
        void BindUBO(GLuint binding_point);

    private:
        struct OpeningData {
            glm::vec4 openings[16]; // xyz = center, w = radius
            int num_openings;
            int padding[3];
        } ubo_data_;

        std::vector<TerrainOpening> openings_;
        GLuint ubo_handle_ = 0;
        bool dirty_ = true;
    };

}
