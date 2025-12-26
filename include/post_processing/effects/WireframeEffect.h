#pragma once

#include "post_processing/IPostProcessingEffect.h"

namespace Boidsish {
namespace PostProcessing {

class WireframeEffect : public IPostProcessingEffect {
public:
    WireframeEffect();
    void Apply(GLuint sourceTexture) override;
    void Initialize(int width, int height) override;
    void Resize(int width, int height) override;
};

}
}
