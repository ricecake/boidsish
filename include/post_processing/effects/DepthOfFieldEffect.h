#pragma once

#include <memory>
#include "post_processing/RenderGraphPostProcessingEffect.h"
#include "post_processing/effects/DepthOfFieldPasses.h"

namespace Boidsish {
    namespace PostProcessing {

        class DepthOfFieldEffect : public RenderGraphPostProcessingEffect {
        public:
            DepthOfFieldEffect();
            ~DepthOfFieldEffect();

            void Initialize(int width, int height) override;
            void Apply(GLuint sourceTexture, GLuint depthTexture, GLuint velocityTexture, GLuint normalTexture, GLuint albedoTexture, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraPos) override;

            void SetFocusDistance(float dist) { manualFocusDistance_ = dist; }
            float GetFocusDistance() const { return manualFocusDistance_; }

            void SetFocusScale(float scale) { focusScale_ = scale; }
            float GetFocusScale() const { return focusScale_; }

            void SetBlurSize(float size) { blurSize_ = size; }
            float GetBlurSize() const { return blurSize_; }

            void SetAutofocus(bool enabled) { autofocusEnabled_ = enabled; }
            bool IsAutofocusEnabled() const { return autofocusEnabled_; }

            void SetFocalPointOffset(const glm::vec2& offset) { focalPointOffset_ = offset; }
            glm::vec2 GetFocalPointOffset() const { return focalPointOffset_; }

            void SetMinFocusDistance(float dist) { minFocusDistance_ = dist; }
            float GetMinFocusDistance() const { return minFocusDistance_; }

            void SetMaxFocusDistance(float dist) { maxFocusDistance_ = dist; }
            float GetMaxFocusDistance() const { return maxFocusDistance_; }

        private:
            DoFParameters params_;
            CoCPass*      cocPass_ = nullptr;
            DoFBlurPass*  blurPass_ = nullptr;

            bool autofocusEnabled_;
            float manualFocusDistance_;
            glm::vec2 focalPointOffset_;
            float minFocusDistance_;
            float maxFocusDistance_;

            float focusScale_;
            float blurSize_;
        };

    } // namespace PostProcessing
} // namespace Boidsish
