#include "post_processing/effects/NegativeEffect.h"
#include "shader.h"

namespace Boidsish {
namespace PostProcessing {

NegativeEffect::NegativeEffect() {
    name_ = "Negative";
}

NegativeEffect::~NegativeEffect() {}

void NegativeEffect::Initialize(int /*width*/, int /*height*/) {
    // This shader only needs the fragment part for the effect.
    // The vertex shader is a standard passthrough, which we can reuse.
    shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/negative.frag");
}

void NegativeEffect::Apply(GLuint sourceTexture) {
    shader_->use();
    shader_->setInt("sceneTexture", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTexture);
    glDrawArrays(GL_TRIANGLES, 0, 6); // Drawing a quad
}

void NegativeEffect::Resize(int /*width*/, int /*height*/) {
    // No specific resizing needed for this effect
}

} // namespace PostProcessing
} // namespace Boidsish
