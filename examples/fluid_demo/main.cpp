#include "graphics.h"
#include "fluid_lbm_manager.h"
#include "model.h"
#include <memory>
#include <iostream>

using namespace Boidsish;

int main() {
    try {
        Visualizer viz(1280, 720, "3D LBM Fluid Demo");

        // Initial camera setup
        Camera cam(0.0f, 10.0f, 40.0f, -15.0f, 0.0f);
        viz.SetCamera(cam);

        FluidLbmManager fluidManager;
        FluidLbmConfig config;
        config.resolution = {64, 64, 64};
        config.worldScale = {40.0f, 40.0f, 40.0f};
        config.worldOrigin = {-20.0f, 2.0f, -20.0f};
        config.gravity = 15.0f;
        config.viscosity = 0.005f;
        fluidManager.Initialize(config);

        // Inject initial fluid
        fluidManager.InjectFluid({0.0f, 30.0f, 0.0f}, 6.0f, 0.5f);

        viz.AddUpdateHandler([&](float totalTime, float dt) {
            fluidManager.Step(dt);

            static float lastInject = 0;
            if (totalTime - lastInject > 8.0f) {
                fluidManager.InjectFluid({0.0f, 30.0f, 0.0f}, 5.0f, 0.5f);
                lastInject = totalTime;
            }
        });

        viz.AddDrawHandler([&](const glm::mat4& view, const glm::mat4& proj, const glm::vec3& camPos) {
            fluidManager.SetTerrainData(viz.GetTerrainHeightmapTexture(), viz.GetTerrainChunkGridTexture(), viz.GetTerrainDataUbo());
            fluidManager.Render(view, proj, camPos, viz.GetDepthTexture());
        });

        viz.Run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
