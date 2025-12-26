#include "SkyboxManager.h"
#include "shader.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Boidsish {
    SkyboxManager::SkyboxManager() {
        shader_ = std::make_unique<Shader>("shaders/sky.vert", "shaders/sky.frag");
        glGenVertexArrays(1, &vao_);
    }

    SkyboxManager::~SkyboxManager() {
        glDeleteVertexArrays(1, &vao_);
    }

    void SkyboxManager::Render(const glm::mat4& projection, const glm::mat4& view) {
        glDisable(GL_DEPTH_TEST);
        shader_->use();
        shader_->setMat4("invProjection", glm::inverse(projection));
        shader_->setMat4("invView", glm::inverse(view));
        glBindVertexArray(vao_);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);
        glEnable(GL_DEPTH_TEST);
    }
}
