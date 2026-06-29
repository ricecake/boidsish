#pragma once

#include <memory>
#include "post_processing/IPostProcessingEffect.h"

class Shader;

namespace Boidsish {
    namespace PostProcessing {

        class DepthOfFieldEffect : public IPostProcessingEffect {
        public:
            DepthOfFieldEffect();
            ~DepthOfFieldEffect();

            void Initialize(int width, int height) override;
            void Apply(GLuint sourceTexture, GLuint depthTexture, GLuint velocityTexture, GLuint normalTexture, GLuint albedoTexture, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraPos) override;
            void Resize(int width, int height) override;

            void SetFocusPoint(float focusPoint) { focusPoint_ = focusPoint; }
            float GetFocusPoint() const { return focusPoint_; }

            void SetFocusScale(float focusScale) { focusScale_ = focusScale; }
            float GetFocusScale() const { return focusScale_; }

            void SetBlurSize(float blurSize) { blurSize_ = blurSize; }
            float GetBlurSize() const { return blurSize_; }

        private:
            std::unique_ptr<Shader> shader_;
            int width_;
            int height_;
            float focusPoint_;
            float focusScale_;
            float blurSize_;
        };

    } // namespace PostProcessing
} // namespace Boidsish
