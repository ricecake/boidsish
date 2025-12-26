#include "post_processing/effects/ShimmeryEffect.h"
#include "shader.h"

namespace Boidsish {
namespace PostProcessing {

ShimmeryEffect::ShimmeryEffect() {
    name_ = "Shimmery";
    shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/shimmery.frag");
}

void ShimmeryEffect::Apply(GLuint sourceTexture) {
    shader_->use();
    shader_->setInt("sceneTexture", 0);
    shader_->setFloat("time", time_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTexture);
}

void ShimmeryEffect::Initialize(int width, int height) {
    // No-op
}

void ShimmeryEffect::Resize(int width, int height) {
    // No-op
}

}
}
