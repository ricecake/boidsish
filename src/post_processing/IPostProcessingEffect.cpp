#include "post_processing/IPostProcessingEffect.h"

namespace Boidsish {
    namespace PostProcessing {

        void IPostProcessingEffect::Initialize(int width, int height) {
            width_ = width;
            height_ = height;
        }

        void IPostProcessingEffect::Resize(int width, int height) {
            width_ = width;
            height_ = height;
        }

    } // namespace PostProcessing
} // namespace Boidsish
