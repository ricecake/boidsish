#include "graphics.h"
#include "terrain_generator.h"
#include "decor_manager.h"
#include "ConfigManager.h"
#include "logger.h"
#include <iostream>

using namespace Boidsish;

int main() {
    try {
        Visualizer viz(1280, 720, "Ambient Camera Overhaul Demo");

        // 1. Setup Environment
        auto terrain = viz.SetTerrainGenerator<TerrainGenerator>(42);
        terrain->SetWorldScale(2.0f);

        auto decor = std::make_shared<DecorManager>();
        decor->PopulateDefaultDecor();
        viz.SetDecorManager(decor);

        // 2. Enable Ambient Mode
        viz.SetCameraMode(CameraMode::AUTO);

        // 3. Configure Camera
        auto& cam = viz.GetCamera();
        cam.x = 0; cam.y = 50; cam.z = 0;

        // 4. Run Visualization
        logger::LOG("Starting ambient overhaul demo. Use '8' to ensure AUTO mode is active.");
        viz.Run();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
