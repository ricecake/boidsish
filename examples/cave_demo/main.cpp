#include "graphics.h"
#include "cave_generator.h"
#include "cave.h"
#include "terrain_generator.h"
#include <iostream>

using namespace Boidsish;

int main() {
    Visualizer vis(1280, 720, "Cave & Varied Terrain Demo");

    // Install a terrain generator with canyons enabled (already default in our new biomes)
    auto terrain = std::make_shared<TerrainGenerator>(42);
    vis.InstallTerrainGenerator(terrain);

    CaveGenerator cave_gen(12345);

    // 1. Create a cave with an opening
    glm::vec3 cave_entrance(50, 0, 50);
    // Find terrain height at entrance
    auto [th, tn] = terrain->GetPointProperties(cave_entrance.x, cave_entrance.z);
    cave_entrance.y = th;

    // Cut a hole in the terrain
    vis.AddTerrainOpening(cave_entrance, 6.0f);

    // Generate the cave mesh
    auto cave_mesh = cave_gen.GenerateCaveMesh(cave_entrance);
    auto cave_entity = std::make_shared<Cave>(cave_mesh);
    vis.AddShape(cave_entity);

    // 2. Create a tunnel through a nearby mountain ridge
    glm::vec3 tunnel_start(-40, 5, -40);
    glm::vec3 tunnel_end(-80, 5, -80);

    vis.AddTerrainOpening(tunnel_start, 5.0f);
    vis.AddTerrainOpening(tunnel_end, 5.0f);

    auto tunnel_mesh = cave_gen.GenerateTunnelMesh(tunnel_start, tunnel_end);
    auto tunnel_entity = std::make_shared<Cave>(tunnel_mesh);
    vis.AddShape(tunnel_entity);

    // Setup camera
    auto& cam = vis.GetCamera();
    cam.x = 70; cam.y = 20; cam.z = 70;
    cam.yaw = 225; cam.pitch = -15;

    vis.Run();

    return 0;
}
