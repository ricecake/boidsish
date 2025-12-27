#include "SkyboxManager.h"
#include "ResourceManager.h"
#include "shader.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Boidsish {

SkyboxManager::SkyboxManager() : vao_(0) {}

SkyboxManager::~SkyboxManager() {
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
    }
}

void SkyboxManager::Initialize(ResourceManager& resourceManager) {
    shader_ = resourceManager.GetShader("sky");
    glGenVertexArrays(1, &vao_);
}

void SkyboxManager::Render(const glm::mat4& view, const glm::mat4& projection) {
    glDisable(GL_DEPTH_TEST);
    shader_->use();
    shader_->setMat4("invProjection", glm::inverse(projection));
    shader_->setMat4("invView", glm::inverse(view));
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}

} // namespace Boidsish
