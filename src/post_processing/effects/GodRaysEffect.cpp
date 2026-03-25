#include "post_processing/effects/GodRaysEffect.h"
#include "ConfigManager.h"
#include "shader.h"

namespace Boidsish {
    namespace PostProcessing {

        GodRaysEffect::GodRaysEffect() {
            name_ = "GodRays";
        }

        GodRaysEffect::~GodRaysEffect() {}

        void GodRaysEffect::Initialize(int width, int height) {
            shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/effects/god_rays.frag");
            width_ = width;
            height_ = height;

            auto& config = ConfigManager::GetInstance();
            is_enabled_ = config.GetAppSettingBool("god_rays_enabled", true);
            samples_ = config.GetAppSettingInt("god_rays_samples", 64);
            density_ = config.GetAppSettingFloat("god_rays_density", 0.99f);
            weight_ = config.GetAppSettingFloat("god_rays_weight", 0.15f);
            decay_ = config.GetAppSettingFloat("god_rays_decay", 0.95f);
            exposure_ = config.GetAppSettingFloat("god_rays_exposure", 0.5f);
        }

        void GodRaysEffect::Apply(
            GLuint sourceTexture,
            GLuint depthTexture,
            GLuint /* velocityTexture */,
            const glm::mat4& viewMatrix,
            const glm::mat4& projectionMatrix,
            const glm::vec3& /* cameraPos */
        ) {
            // We use the quad VAO provided by the PostProcessingManager via glBindVertexArray(quad_vao_)
            // in PostProcessingManager::ApplyEffectInternal.

            shader_->use();
            shader_->setInt("sceneTexture", 0);
            shader_->setInt("depthTexture", 1);
            shader_->setVec3("sunDir", sun_dir_);
            shader_->setMat4("view", viewMatrix);
            shader_->setMat4("projection", projectionMatrix);

            shader_->setInt("samples", samples_);
            shader_->setFloat("density", density_);
            shader_->setFloat("weight", weight_);
            shader_->setFloat("decay", decay_);
            shader_->setFloat("exposure", exposure_);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, sourceTexture);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, depthTexture);

            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        void GodRaysEffect::Resize(int width, int height) {
            width_ = width;
            height_ = height;
        }

    } // namespace PostProcessing
} // namespace Boidsish
