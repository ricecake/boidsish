#pragma once

#include "post_processing/IPostProcessingEffect.h"

namespace Boidsish {
    namespace PostProcessing {

        class BlackAndWhiteEffect : public IPostProcessingEffect {
        public:
            void Initialize(int width, int height) override;
            void Apply(GLuint texture) override;
            void Resize(int width, int height) override;
        };

    } // namespace PostProcessing
} // namespace Boidsish
