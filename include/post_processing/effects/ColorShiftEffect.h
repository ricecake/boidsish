#pragma once

#include "post_processing/IPostProcessingEffect.h"

namespace Boidsish {
namespace PostProcessing {

class ColorShiftEffect : public IPostProcessingEffect {
public:
    ColorShiftEffect();
    void Apply(GLuint sourceTexture) override;
    void Initialize(int width, int height) override;
    void Resize(int width, int height) override;
};

}
}
