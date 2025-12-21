#pragma once

#include <vector>

#include "shape.h"
#include <glm/glm.hpp>

namespace Boidsish {

    class PointCloud : public Shape {
    public:
        PointCloud(const std::vector<float>& vertexData);
        ~PointCloud();

        void render() const override;

        static std::shared_ptr<Shader> point_cloud_shader_;

    private:
        void setupMesh(const std::vector<float>& vertexData);

        unsigned int vao_, vbo_;
        int          vertex_count_;
    };

} // namespace Boidsish
