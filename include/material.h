#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include "handle.h"

namespace Boidsish {

    /**
     * @brief Structure representing a material's visual properties.
     * In a data-driven system, these properties are used to populate shader uniforms.
     */
    struct Material {
        glm::vec3 color = glm::vec3(1.0f);
        float     alpha = 1.0f;

        // PBR Properties
        float roughness = 0.5f;
        float metallic = 0.0f;
        float ao = 1.0f;

        // Texture information
        struct TextureInfo {
            unsigned int id;
            std::string  type;
        };
        std::vector<TextureInfo> textures;

        // Optional name for debugging
        std::string name;
    };

    /**
     * @brief A type-safe handle for referring to Material resources.
     */
    using MaterialHandle = Handle<Material>;

} // namespace Boidsish
