#pragma once

#include <memory>
#include "model.h" // For Vertex and Mesh structs
#include "shape.h"

namespace Boidsish {

class ProceduralHead : public Shape {
public:
    ProceduralHead();
    void render() const override;

    // Deformation parameters
    float eye_size = 1.0f;
    float eye_separation = 0.0f;
    float chin_size = 1.0f;
    float nose_size = 1.0f;
    float nose_length = 0.0f;
    float cheek_depth = 0.0f;
    float ear_height = 0.0f;
    float brow_height = 0.0f;
    float brow_width = 1.0f;

    // Apply deformations to the mesh
    void deform_mesh();

private:
    void generate_head_mesh(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices);
    void update_buffers();

    std::unique_ptr<Mesh> mesh_;
    // Original vertex data
    std::vector<Vertex> original_vertices_;
};

} // namespace Boidsish
