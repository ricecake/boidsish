#pragma once

#include <vector>

#include "shape.h"
#include <glm/glm.hpp>

namespace Boidsish {
    class Terrain : public Shape {
    public:
        Terrain(const std::vector<float>& vertexData,
                const std::vector<unsigned int>& indices,
                std::vector<std::vector<glm::vec3>> heightmap);
        ~Terrain();

        void render() const override;

        const std::vector<std::vector<glm::vec3>>& GetHeightmap() const {
            return heightmap_;
        }

        static std::shared_ptr<Shader> terrain_shader_;

    private:
        void setupMesh(const std::vector<float>& vertexData,
                       const std::vector<unsigned int>& indices);

        unsigned int vao_, vbo_, ebo_;
        int index_count_;
        std::vector<std::vector<glm::vec3>> heightmap_;
    };

}  // namespace Boidsish
