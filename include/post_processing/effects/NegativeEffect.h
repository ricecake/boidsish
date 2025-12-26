#pragma once

#include "post_processing/IPostProcessingEffect.h"
#include <memory>

class Shader; // Forward declaration

namespace Boidsish {
namespace PostProcessing {

class NegativeEffect : public IPostProcessingEffect {
public:
    NegativeEffect();
    ~NegativeEffect();

    void Apply(GLuint sourceTexture) override;
    void Initialize(int width, int height) override;
    void Resize(int width, int height) override;

private:
    std::unique_ptr<Shader> shader_;
};

} // namespace PostProcessing
} // namespace Boidsish
