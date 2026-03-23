#include <iostream>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "graphics.h"
#include "logger.h"
#include "fire_effect.h"
#include "post_processing/PostProcessingManager.h"
#include "post_processing/effects/VoxelVolumeEffect.h"

using namespace Boidsish;

int main() {
    try {
        Visualizer vis(1280, 720, "Voxel Brick System Demo");
        vis.GetCamera().y = 10.0f;
        vis.GetCamera().z = 30.0f;
        vis.SetCameraMode(CameraMode::FREE);

        // 1. Setup Emitters

        // Moving fire emitter
        auto fire = vis.AddFireEffect(
            glm::vec3(-10.0f, 2.0f, 0.0f),
            FireEffectStyle::Fire,
            glm::vec3(0, 1, 0),
            glm::vec3(0),
            1000
        );

        // Moving smoke emitter (using Bubbles for visual contrast if Smoke doesn't exist)
        auto smoke = vis.AddFireEffect(
            glm::vec3(10.0f, 2.0f, 0.0f),
            FireEffectStyle::Bubbles,
            glm::vec3(0, 1, 0),
            glm::vec3(0),
            1000
        );

        // Stationary explosion (triggered periodically)
        glm::vec3 explosion_pos(0.0f, 2.0f, -10.0f);
        float explosion_timer = 0.0f;

        vis.AddShapeHandler([&](float time) {
            // Update fire position
            fire->SetPosition(glm::vec3(-10.0f + sin(time * 0.5f) * 5.0f, 2.0f, cos(time * 0.5f) * 5.0f));

            // Update smoke position
            smoke->SetPosition(glm::vec3(10.0f + cos(time * 0.7f) * 5.0f, 2.0f, sin(time * 0.7f) * 5.0f));

            // Periodic explosion
            if (time - explosion_timer > 4.0f) {
                vis.CreateExplosion(explosion_pos, 2.0f);
                explosion_timer = time;
            }

            return std::vector<std::shared_ptr<Shape>>{};
        });

        // 2. Configure Voxel Volume Effect
        auto& ppm = vis.GetPostProcessingManager();
        for (auto& effect : ppm.GetPreToneMappingEffects()) {
            if (auto voxel_effect = std::dynamic_pointer_cast<PostProcessing::VoxelVolumeEffect>(effect)) {
                voxel_effect->SetEnabled(true);
                voxel_effect->SetStepSize(0.2f);
                voxel_effect->SetDensityScale(2.5f);
                voxel_effect->SetAmbientColor(glm::vec3(0.02f, 0.02f, 0.05f));
            }
        }

        vis.Run();

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
