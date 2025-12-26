#pragma once

#include "post_processing/IPostProcessingEffect.h"

namespace Boidsish {
namespace PostProcessing {

class BlackAndWhiteEffect : public IPostProcessingEffect {
public:
    BlackAndWhiteEffect();
    void Apply(GLuint sourceTexture) override;
    void Initialize(int width, int height) override;
    void Resize(int width, int height) override;
};

}
}
