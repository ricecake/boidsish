#include "graphics.h"
#include "cave_generator.h"
#include "cave.h"
#include "terrain_generator.h"
#include <iostream>

using namespace Boidsish;

int main() {
    Visualizer vis(1280, 720, "Cave & Varied Terrain Demo");

    // Install a terrain generator with canyons enabled
    auto terrain = std::make_shared<TerrainGenerator>(42);
    vis.InstallTerrainGenerator(terrain);

    CaveGenerator cave_gen(12345);

    // 1. Create a cave with an opening
    glm::vec3 cave_entrance(50, 0, 50);
    auto [th, tn] = terrain->GetPointProperties(cave_entrance.x, cave_entrance.z);
    cave_entrance.y = th;

    // Cut a hole in the terrain
    vis.AddTerrainOpening(cave_entrance, 7.0f);

    // Generate the cave mesh with higher resolution (cell_size = 0.5)
    std::cout << "Generating cave mesh..." << std::endl;
    auto cave_mesh = cave_gen.GenerateCaveMesh(cave_entrance, 60.0f, 0.6f);
    auto cave_entity = std::make_shared<Cave>(cave_mesh);
    vis.AddShape(cave_entity);

    // 2. Create a tunnel through a nearby mountain ridge
    glm::vec3 tunnel_start(-50, 5, -50);
    glm::vec3 tunnel_end(-100, 5, -100);

    vis.AddTerrainOpening(tunnel_start, 6.0f);
    vis.AddTerrainOpening(tunnel_end, 6.0f);

    std::cout << "Generating tunnel mesh..." << std::endl;
    auto tunnel_mesh = cave_gen.GenerateTunnelMesh(tunnel_start, tunnel_end, 0.7f);
    auto tunnel_entity = std::make_shared<Cave>(tunnel_mesh);
    vis.AddShape(tunnel_entity);

    // Setup camera
    auto& cam = vis.GetCamera();
    cam.x = 80; cam.y = 30; cam.z = 80;
    cam.yaw = 225; cam.pitch = -25;
    cam.speed = 30.0f;

    std::cout << "Running demo. Press ESC to exit." << std::endl;
    vis.Run();

    return 0;
}
