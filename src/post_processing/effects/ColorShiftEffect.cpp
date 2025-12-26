#include "post_processing/effects/ColorShiftEffect.h"
#include "shader.h"

namespace Boidsish {
namespace PostProcessing {

ColorShiftEffect::ColorShiftEffect() {
    name_ = "Color Shift";
    shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/colorshift.frag");
}

void ColorShiftEffect::Apply(GLuint sourceTexture) {
    shader_->use();
    shader_->setInt("sceneTexture", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTexture);
}

void ColorShiftEffect::Initialize(int width, int height) {
    // No-op
}

void ColorShiftEffect::Resize(int width, int height) {
    // No-op
}

}
}
