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
    glm::vec3 cave_entrance(60, 0, 60);
    auto [th, tn] = terrain->GetPointProperties(cave_entrance.x, cave_entrance.z);
    cave_entrance.y = th;

    // Cut a hole in the terrain
    vis.AddTerrainOpening(cave_entrance, 8.0f);

    // Generate the cave mesh
    std::cout << "Generating cave mesh..." << std::endl;
    auto cave_mesh = cave_gen.GenerateCaveMesh(cave_entrance, 70.0f, 0.75f);
    auto cave_entity = std::make_shared<Cave>(cave_mesh);
    vis.AddShape(cave_entity);

    // 2. Create a tunnel through a nearby mountain ridge
    glm::vec3 tunnel_start(-60, 5, -60);
    glm::vec3 tunnel_end(-110, 5, -110);

    vis.AddTerrainOpening(tunnel_start, 7.0f);
    vis.AddTerrainOpening(tunnel_end, 7.0f);

    std::cout << "Generating tunnel mesh..." << std::endl;
    auto tunnel_mesh = cave_gen.GenerateTunnelMesh(tunnel_start, tunnel_end, 0.8f);
    auto tunnel_entity = std::make_shared<Cave>(tunnel_mesh);
    vis.AddShape(tunnel_entity);

    // Setup camera
    auto& cam = vis.GetCamera();
    cam.x = 90; cam.y = 40; cam.z = 90;
    cam.yaw = 225; cam.pitch = -30;
    cam.speed = 30.0f;

    std::cout << "Running demo. Press ESC to exit." << std::endl;
    vis.Run();

    return 0;
}
