#pragma once

#include <glm/glm.hpp>
#include "frustum.h"
#include "shader_table.h"

namespace Boidsish {

    /**
     * @brief Holds frame-level rendering state and context for geometry generation.
     * This information is generally available to all shapes during packet generation.
     */
    struct RenderContext {
        glm::mat4 view = glm::mat4(1.0f);
        glm::mat4 projection = glm::mat4(1.0f);
        glm::vec3 view_pos = glm::vec3(0.0f);
        float     far_plane = 1000.0f;
        float     time = 0.0f;
        Frustum   frustum;
        const ShaderTable* shader_table = nullptr;

        // Optional: helper to project a world position to screen space or calculate depth
        float CalculateNormalizedDepth(const glm::vec3& world_pos) const {
            float depth = glm::distance(view_pos, world_pos);
            return glm::clamp(depth / far_plane, 0.0f, 1.0f);
        }
    };

} // namespace Boidsish
