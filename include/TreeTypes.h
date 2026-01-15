#pragma once

#include <glm/glm.hpp>

namespace Boidsish {

// GLSL-compatible structs (std140/std430 layout)
struct AttractionPoint {
    glm::vec4 position;
    int is_active;
    glm::vec3 padding; // Explicit padding for alignment
};

struct Branch {
    glm::vec4 position;
    glm::vec4 parent_position;
    int parent_index;
    float thickness;
    glm::vec2 padding; // Explicit padding for alignment
};

} // namespace Boidsish
