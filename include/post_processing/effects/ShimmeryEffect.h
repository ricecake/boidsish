#pragma once

#include "post_processing/IPostProcessingEffect.h"

namespace Boidsish {
namespace PostProcessing {

class ShimmeryEffect : public IPostProcessingEffect {
public:
    ShimmeryEffect();
    void Apply(GLuint sourceTexture) override;
    void Initialize(int width, int height) override;
    void Resize(int width, int height) override;

    void SetTime(float time) { time_ = time; }

private:
    float time_ = 0.0f;
};

}
}
