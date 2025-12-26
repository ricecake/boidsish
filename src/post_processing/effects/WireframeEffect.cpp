#include "post_processing/effects/WireframeEffect.h"
#include "shader.h"

namespace Boidsish {
namespace PostProcessing {

WireframeEffect::WireframeEffect() {
    name_ = "Wireframe";
    shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/wireframe.frag");
}

void WireframeEffect::Apply(GLuint sourceTexture) {
    shader_->use();
    shader_->setInt("sceneTexture", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTexture);
}

void WireframeEffect::Initialize(int width, int height) {
    // No-op
}

void WireframeEffect::Resize(int width, int height) {
    // No-op
}

}
}
