#include "post_processing/effects/FXAAEffect.h"
#include "shader.h"

namespace Boidsish {
    namespace PostProcessing {

        FXAAEffect::FXAAEffect()
            : reduceMin_(1.0f/128.0f), reduceMul_(1.0f/8.0f), spanMax_(8.0f),
              lumaThreshold_(0.01f), mulAlpha_(false), width_(0), height_(0) {
            name_ = "FXAA";
            is_enabled_ = false;
        }

        FXAAEffect::~FXAAEffect() {}

        void FXAAEffect::Initialize(int width, int height) {
            width_ = width;
            height_ = height;
            shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/post_processing/fxaa.frag");
            shader_->use();
            shader_->setInt("screenTexture", 0);
        }

        void FXAAEffect::Apply(GLuint sourceTexture, GLuint depthTexture, GLuint velocityTexture, GLuint normalTexture, GLuint albedoTexture, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraPos) {
            shader_->use();
            shader_->setVec2("RESOLUTION", glm::vec2(width_, height_));
            shader_->setFloat("uReduceMin", reduceMin_);
            shader_->setFloat("uReduceMul", reduceMul_);
            shader_->setFloat("uSpanMax", spanMax_);
            shader_->setFloat("uLumaThreshold", lumaThreshold_);
            shader_->setBool("uMulAlpha", mulAlpha_);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, sourceTexture);

            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        void FXAAEffect::Resize(int width, int height) {
            width_ = width;
            height_ = height;
        }

    } // namespace PostProcessing
} // namespace Boidsish
