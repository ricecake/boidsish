#pragma once

#include <memory>
#include "post_processing/IPostProcessingEffect.h"

class Shader;

namespace Boidsish {
    namespace PostProcessing {

        class GodRaysEffect : public IPostProcessingEffect {
        public:
            GodRaysEffect();
            ~GodRaysEffect();

            void Apply(
                GLuint           sourceTexture,
                GLuint           depthTexture,
                GLuint           velocityTexture,
                const glm::mat4& viewMatrix,
                const glm::mat4& projectionMatrix,
                const glm::vec3& cameraPos
            ) override;
            void Initialize(int width, int height) override;
            void Resize(int width, int height) override;

            void SetTime(float time) override { time_ = time; }
            bool IsEarly() const override { return false; } // Apply after other effects, before tone mapping if possible

            void SetSunDir(const glm::vec3& dir) { sun_dir_ = dir; }
            void SetSamples(int samples) { samples_ = samples; }
            void SetDensity(float density) { density_ = density; }
            void SetWeight(float weight) { weight_ = weight; }
            void SetDecay(float decay) { decay_ = decay; }
            void SetExposure(float exposure) { exposure_ = exposure; }

            int GetSamples() const { return samples_; }
            float GetDensity() const { return density_; }
            float GetWeight() const { return weight_; }
            float GetDecay() const { return decay_; }
            float GetExposure() const { return exposure_; }

        private:
            std::unique_ptr<Shader> shader_;
            float                   time_ = 0.0f;
            glm::vec3               sun_dir_ = glm::vec3(0.0f, 1.0f, 0.0f);

            int   samples_ = 32;
            float density_ = 0.96f;
            float weight_ = 0.58f;
            float decay_ = 0.9f;
            float exposure_ = 0.2f;

            int width_ = 0;
            int height_ = 0;
        };

    } // namespace PostProcessing
} // namespace Boidsish
