#include "post_processing/effects/BlackAndWhiteEffect.h"

#include <shader.h>

namespace Boidsish {
    namespace PostProcessing {

        void BlackAndWhiteEffect::Initialize(int width, int height) {
            IPostProcessingEffect::Initialize(width, height);
            shader_ = std::make_unique<Shader>("shaders/post_processing/fullscreen.vert", "shaders/post_processing/black_and_white.frag");
        }

        void BlackAndWhiteEffect::Apply(GLuint texture) {
            shader_->use();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture);
            shader_->setInt("sceneTexture", 0);
        }

        void BlackAndWhiteEffect::Resize(int width, int height) {
            IPostProcessingEffect::Resize(width, height);
        }

    } // namespace PostProcessing
} // namespace Boidsish
