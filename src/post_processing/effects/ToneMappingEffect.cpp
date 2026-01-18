#include "post_processing/effects/ToneMappingEffect.h"

#include "shader.h"

namespace Boidsish {
namespace PostProcessing {

ToneMappingEffect::ToneMappingEffect() {
    name_ = "ToneMapping";
}

ToneMappingEffect::~ToneMappingEffect() {
}

void ToneMappingEffect::Initialize(int width, int height) {
    _shader = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/tonemapping.frag");
}

void ToneMappingEffect::Apply(GLuint sourceTexture) {
    _shader->use();
    _shader->setInt("sceneTexture", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTexture);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void ToneMappingEffect::Resize(int width, int height) {
    // No-op
}

} // namespace PostProcessing
} // namespace Boidsish
