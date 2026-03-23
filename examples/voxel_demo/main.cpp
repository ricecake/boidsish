#include <iostream>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "graphics.h"
#include "logger.h"
#include "fire_effect.h"
#include "fire_effect_manager.h"
#include "post_processing/PostProcessingManager.h"
#include "post_processing/effects/VoxelVolumeEffect.h"

using namespace Boidsish;

int main() {
    try {
        Visualizer vis(1280, 720, "Voxel Multi-Volume Demo");
        vis.GetCamera().y = 10.0f;
        vis.GetCamera().z = 35.0f;
        vis.SetCameraMode(CameraMode::FREE);

        // 1. Setup Emitters

        // Fire particles (style 2)
        auto fire = vis.AddFireEffect(
            glm::vec3(-12.0f, 2.0f, 0.0f),
            FireEffectStyle::Fire,
            glm::vec3(0, 1, 0),
            glm::vec3(0),
            2000
        );

        // Smoke/Bubble particles (style 6)
        auto smoke = vis.AddFireEffect(
            glm::vec3(12.0f, 2.0f, 0.0f),
            FireEffectStyle::Bubbles,
            glm::vec3(0, 1, 0),
            glm::vec3(0),
            2000
        );

        // Stationary explosion (style 1)
        glm::vec3 explosion_pos(0.0f, 2.0f, -15.0f);
        float explosion_timer = 0.0f;

        vis.AddShapeHandler([&](float time) {
            fire->SetPosition(glm::vec3(-12.0f + sin(time * 0.4f) * 8.0f, 2.0f, cos(time * 0.4f) * 8.0f));
            smoke->SetPosition(glm::vec3(12.0f + cos(time * 0.6f) * 8.0f, 2.0f, sin(time * 0.6f) * 8.0f));

            if (time - explosion_timer > 5.0f) {
                vis.CreateExplosion(explosion_pos, 2.5f);
                explosion_timer = time;
            }

            return std::vector<std::shared_ptr<Shape>>{};
        });

        // 2. Configure Fire Voxel Buffer (Buffer 0)
        auto fem = vis.GetFireEffectManager();
        fem->SetVoxelStyleMask(0, (1 << (int)FireEffectStyle::Fire) | (1 << (int)FireEffectStyle::Explosion));

        // 3. Configure Smoke Voxel Buffer (Buffer 1)
        fem->SetVoxelStyleMask(1, (1 << (int)FireEffectStyle::Bubbles));

        // 4. Configure Post-Processing Effects
        auto& ppm = vis.GetPostProcessingManager();

        // Effect for Fire/Explosion (Buffer 0)
        auto fire_voxel = std::make_shared<PostProcessing::VoxelVolumeEffect>();
        fire_voxel->SetEnabled(true);
        fire_voxel->SetStepSize(0.15f);
        fire_voxel->SetDensityScale(2.0f);
        fire_voxel->SetAmbientColor(glm::vec3(0.1f, 0.02f, 0.0f)); // Reddish ambient
        fire_voxel->SetUnitOffset(0);
        ppm.AddEffect(fire_voxel);

        // Effect for Smoke (Buffer 1)
        auto smoke_voxel = std::make_shared<PostProcessing::VoxelVolumeEffect>();
        smoke_voxel->SetEnabled(true);
        smoke_voxel->SetStepSize(0.3f);
        smoke_voxel->SetDensityScale(1.0f);
        smoke_voxel->SetAmbientColor(glm::vec3(0.05f, 0.05f, 0.08f)); // Bluish ambient
        smoke_voxel->SetUnitOffset(1);
        ppm.AddEffect(smoke_voxel);

        vis.Run();

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
