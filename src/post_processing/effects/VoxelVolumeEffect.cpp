#include "post_processing/effects/VoxelVolumeEffect.h"
#include <GL/glew.h>
#include "constants.h"

namespace Boidsish {
    namespace PostProcessing {

        VoxelVolumeEffect::VoxelVolumeEffect() {
            name_ = "VoxelVolume";
        }

        void VoxelVolumeEffect::Initialize(int width, int height) {
            shader_ = std::make_shared<Shader>("shaders/postprocess.vert", "shaders/post_processing/voxel_volume.frag");
        }

        void VoxelVolumeEffect::Resize(int width, int height) {}

        void VoxelVolumeEffect::Apply(
            GLuint           sourceTexture,
            GLuint           depthTexture,
            GLuint           velocityTexture,
            const glm::mat4& viewMatrix,
            const glm::mat4& projectionMatrix,
            const glm::vec3& cameraPos
        ) {
            shader_->use();

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, sourceTexture);
            shader_->setInt("u_inputTexture", 0);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, depthTexture);
            shader_->setInt("u_depthTexture", 1);

            shader_->setFloat("u_stepSize", step_size_);
            shader_->setFloat("u_maxDistance", max_dist_);
            shader_->setFloat("u_densityScale", density_scale_);
            shader_->setVec3("u_ambientColor", ambient_color_);
            shader_->setVec3("u_cameraPos", cameraPos);
            shader_->setMat4("u_invProj", glm::inverse(projectionMatrix));
            shader_->setMat4("u_invView", glm::inverse(viewMatrix));

            shader_->setInt("u_brickPool", Constants::Class::VoxelBricks::BrickPoolUnit());

            // Draw full screen quad
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

    }
}
