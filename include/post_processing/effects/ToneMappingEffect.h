#pragma once

#include "post_processing/IPostProcessingEffect.h"
#include <memory>

#include "shader.h"

namespace Boidsish {
namespace PostProcessing {

class ToneMappingEffect : public IPostProcessingEffect {
public:
    ToneMappingEffect();
    ~ToneMappingEffect();

    void Initialize(int width, int height) override;
    void Apply(GLuint sourceTexture) override;
    void Resize(int width, int height) override;

private:
    std::unique_ptr<Shader> _shader;
};

} // namespace PostProcessing
} // namespace Boidsish
