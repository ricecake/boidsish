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
        Camera cam(0.0f, 15.0f, 35.0f, -20.0f, 0.0f);
        viz.SetCamera(cam);

        FluidLbmManager fluidManager;
        FluidLbmConfig config;
        config.resolution = {32, 32, 32}; // Balanced for performance
        config.worldScale = {30.0f, 30.0f, 30.0f};
        config.worldOrigin = {-15.0f, -2.0f, -15.0f};
        config.gravity = 25.0f; // Fast drop
        config.viscosity = 0.005f;
        fluidManager.Initialize(config);

        // Inject initial fluid
        fluidManager.InjectFluid({0.0f, 20.0f, 0.0f}, 4.0f, 0.5f);

        viz.AddUpdateHandler([&](float totalTime, float dt) {
            // Cap delta time for simulation stability
            float simDt = std::min(dt, 0.033f);
            fluidManager.Step(simDt);

            static float lastInject = 0;
            if (totalTime - lastInject > 6.0f) {
                fluidManager.InjectFluid({0.0f, 20.0f, 0.0f}, 4.0f, 0.5f);
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
