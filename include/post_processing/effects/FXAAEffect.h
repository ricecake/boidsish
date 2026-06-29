#pragma once

#include <memory>
#include "post_processing/IPostProcessingEffect.h"

class Shader;

namespace Boidsish {
    namespace PostProcessing {

        class FXAAEffect : public IPostProcessingEffect {
        public:
            FXAAEffect();
            ~FXAAEffect();

            void Initialize(int width, int height) override;
            void Apply(GLuint sourceTexture, GLuint depthTexture, GLuint velocityTexture, GLuint normalTexture, GLuint albedoTexture, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraPos) override;
            void Resize(int width, int height) override;

            void SetReduceMin(float val) { reduceMin_ = val; }
            float GetReduceMin() const { return reduceMin_; }

            void SetReduceMul(float val) { reduceMul_ = val; }
            float GetReduceMul() const { return reduceMul_; }

            void SetSpanMax(float val) { spanMax_ = val; }
            float GetSpanMax() const { return spanMax_; }

        private:
            std::unique_ptr<Shader> shader_;
            int width_;
            int height_;
            float reduceMin_;
            float reduceMul_;
            float spanMax_;
        };

    } // namespace PostProcessing
} // namespace Boidsish
