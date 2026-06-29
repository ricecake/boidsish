#include "post_processing/effects/DepthOfFieldEffect.h"
#include "shader.h"
#include <GLFW/glfw3.h>

namespace Boidsish {
    namespace PostProcessing {

        DepthOfFieldEffect::DepthOfFieldEffect()
            : focusPoint_(0.5f), focusScale_(5.0f), blurSize_(6.0f), width_(0), height_(0) {
            name_ = "Depth of Field";
            is_enabled_ = false;
        }

        DepthOfFieldEffect::~DepthOfFieldEffect() {}

        void DepthOfFieldEffect::Initialize(int width, int height) {
            width_ = width;
            height_ = height;
            shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/post_processing/dof.frag");
            shader_->use();
            shader_->setInt("screenTexture", 0);
            shader_->setInt("depthTexture", 1);
        }

        void DepthOfFieldEffect::Apply(GLuint sourceTexture, GLuint depthTexture, GLuint velocityTexture, GLuint normalTexture, GLuint albedoTexture, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraPos) {
            shader_->use();
            shader_->setVec2("RESOLUTION", glm::vec2(width_, height_));
            shader_->setFloat("uFocusPoint", focusPoint_);
            shader_->setFloat("uFocusScale", focusScale_);
            shader_->setFloat("uBlurSize", blurSize_);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, sourceTexture);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, depthTexture);

            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        void DepthOfFieldEffect::Resize(int width, int height) {
            width_ = width;
            height_ = height;
        }

    } // namespace PostProcessing
} // namespace Boidsish
