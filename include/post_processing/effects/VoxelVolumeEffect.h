#pragma once

#include "post_processing/IPostProcessingEffect.h"
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include "shader.h"

namespace Boidsish {
    namespace PostProcessing {

        class VoxelVolumeEffect : public IPostProcessingEffect {
        public:
            VoxelVolumeEffect();
            virtual ~VoxelVolumeEffect() = default;

            void Initialize(int width, int height) override;
            void Resize(int width, int height) override;
            void Apply(
                GLuint           sourceTexture,
                GLuint           depthTexture,
                GLuint           velocityTexture,
                const glm::mat4& viewMatrix,
                const glm::mat4& projectionMatrix,
                const glm::vec3& cameraPos
            ) override;

            void SetStepSize(float size) { step_size_ = size; }
            void SetMaxDistance(float dist) { max_dist_ = dist; }
            void SetDensityScale(float scale) { density_scale_ = scale; }
            void SetAmbientColor(const glm::vec3& color) { ambient_color_ = color; }
            void SetStyleMask(uint32_t mask) { style_mask_ = mask; }
            void SetUnitOffset(int offset) { unit_offset_ = offset; }

        private:
            std::shared_ptr<Shader> shader_;
            float    step_size_ = 0.5f;
            float    max_dist_ = 500.0f;
            float    density_scale_ = 1.0f;
            glm::vec3 ambient_color_ = glm::vec3(0.1f, 0.1f, 0.15f);
            uint32_t  style_mask_ = 0xFFFFFFFF; // Bitmask of styles to include
            int       unit_offset_ = 0;
        };

    }
}
