#pragma once

#include "visual_effects.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <memory>

class Shader;

namespace Boidsish {
    class ResourceManager {
    public:
        ResourceManager();
        ~ResourceManager();

        void BindUBOs(Shader& shader);
        void UpdateLightingUBO(const glm::vec3& light_pos, const glm::vec3& view_pos, float time);
        void UpdateVisualEffectsUBO(const VisualEffectsUbo& ubo_data);

    private:
        GLuint lighting_ubo_;
        GLuint visual_effects_ubo_;
    };
}
