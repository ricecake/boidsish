#include "ResourceManager.h"
#include "shader.h"

namespace Boidsish {
    ResourceManager::ResourceManager() {
        glGenBuffers(1, &lighting_ubo_);
        glBindBuffer(GL_UNIFORM_BUFFER, lighting_ubo_);
        glBufferData(GL_UNIFORM_BUFFER, 48, NULL, GL_STATIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        glBindBufferRange(GL_UNIFORM_BUFFER, 0, lighting_ubo_, 0, 48);

        glGenBuffers(1, &visual_effects_ubo_);
        glBindBuffer(GL_UNIFORM_BUFFER, visual_effects_ubo_);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(VisualEffectsUbo), NULL, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        glBindBufferRange(GL_UNIFORM_BUFFER, 1, visual_effects_ubo_, 0, sizeof(VisualEffectsUbo));
    }

    ResourceManager::~ResourceManager() {
        glDeleteBuffers(1, &lighting_ubo_);
        glDeleteBuffers(1, &visual_effects_ubo_);
    }

    void ResourceManager::BindUBOs(Shader& shader) {
        shader.use();
        glUniformBlockBinding(shader.ID, glGetUniformBlockIndex(shader.ID, "Lighting"), 0);
        glUniformBlockBinding(shader.ID, glGetUniformBlockIndex(shader.ID, "VisualEffects"), 1);
    }

    void ResourceManager::UpdateLightingUBO(const glm::vec3& light_pos, const glm::vec3& view_pos, float time) {
        glBindBuffer(GL_UNIFORM_BUFFER, lighting_ubo_);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::vec3), &light_pos[0]);
        glBufferSubData(GL_UNIFORM_BUFFER, 16, sizeof(glm::vec3), &view_pos[0]);
        glBufferSubData(GL_UNIFORM_BUFFER, 32, sizeof(glm::vec3), &glm::vec3(1.0f, 1.0f, 1.0f)[0]);
        glBufferSubData(GL_UNIFORM_BUFFER, 44, sizeof(float), &time);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    void ResourceManager::UpdateVisualEffectsUBO(const VisualEffectsUbo& ubo_data) {
        glBindBuffer(GL_UNIFORM_BUFFER, visual_effects_ubo_);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(VisualEffectsUbo), &ubo_data);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }
}
