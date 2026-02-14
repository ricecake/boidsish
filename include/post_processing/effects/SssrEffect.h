#pragma once

#include "post_processing/IPostProcessingEffect.h"
#include <memory>
#include <shader.h>

namespace Boidsish::PostProcessing {

    class SssrEffect : public IPostProcessingEffect {
    public:
        SssrEffect();
        ~SssrEffect() override;

        void Apply(const PostProcessingParams& params) override;
        void Initialize(int width, int height) override;
        void Resize(int width, int height) override;

    private:
        std::unique_ptr<Shader> sssr_shader_;
        int                     width_ = 1280;
        int                     height_ = 720;
    };

} // namespace Boidsish::PostProcessing
