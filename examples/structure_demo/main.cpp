#include "graphics.h"
#include "mesh_terrain_render_manager.h"
#include "structure_terrain_generator.h"
#include "logger.h"

using namespace Boidsish;

int main() {
    try {
        Visualizer visualizer(1280, 720, "Structure Terrain Demo");

        // 1. Create the new mesh-based renderer
        auto renderer = std::make_shared<MeshTerrainRenderManager>(32);
        visualizer.SetTerrainRenderManager(renderer);

        // 2. Install the structure generator
        auto generator = visualizer.SetTerrainGenerator<StructureTerrainGenerator>(54321);

        // 3. Setup camera to be inside the structure
        Camera cam;
        cam.x = 16.0f;
        cam.y = 5.0f;
        cam.z = 16.0f;
        cam.pitch = 0.0f;
        cam.yaw = 0.0f;
        visualizer.SetCamera(cam);
        visualizer.SetCameraMode(CameraMode::FREE);

        // 4. Run
        visualizer.Run();

    } catch (const std::exception& e) {
        logger::ERROR("Exception in demo: " + std::string(e.what()));
        return 1;
    }

    return 0;
}
