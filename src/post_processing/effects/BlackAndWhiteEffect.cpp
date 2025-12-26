#include "post_processing/effects/BlackAndWhiteEffect.h"
#include "shader.h"

namespace Boidsish {
namespace PostProcessing {

BlackAndWhiteEffect::BlackAndWhiteEffect() {
    name_ = "Black and White";
    shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/blackandwhite.frag");
}

void BlackAndWhiteEffect::Apply(GLuint sourceTexture) {
    shader_->use();
    shader_->setInt("sceneTexture", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTexture);
}

void BlackAndWhiteEffect::Initialize(int width, int height) {
    // No-op
}

void BlackAndWhiteEffect::Resize(int width, int height) {
    // No-op
}

}
}
