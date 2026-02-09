#include "graphics.h"
#include "cave_generator.h"
#include "cave.h"
#include "terrain_generator.h"
#include <iostream>

using namespace Boidsish;

// Find a ridge point and determine tunnel endpoints on opposite sides
struct TunnelPath {
    glm::vec3 start;
    glm::vec3 end;
    glm::vec3 ridge_center;
    bool found;
};

TunnelPath FindTunnelThroughRidge(TerrainGenerator& terrain, float search_x, float search_z) {
    TunnelPath result;
    result.found = false;

    // First, find the highest point in the search area (the ridge)
    float best_height = 0.0f;
    glm::vec3 ridge_pos(search_x, 0, search_z);

    for (float dx = -40; dx <= 40; dx += 5) {
        for (float dz = -40; dz <= 40; dz += 5) {
            float x = search_x + dx;
            float z = search_z + dz;
            auto [h, n] = terrain.GetPointProperties(x, z);
            if (h > best_height) {
                best_height = h;
                ridge_pos = glm::vec3(x, h, z);
            }
        }
    }

    // Need at least some elevation for a meaningful tunnel
    if (best_height < 8.0f) {
        return result;
    }

    result.ridge_center = ridge_pos;

    // Now find the direction of steepest descent to determine tunnel orientation
    // Sample terrain gradient around the ridge
    float grad_x = 0, grad_z = 0;
    float sample_dist = 15.0f;

    auto [h_px, n1] = terrain.GetPointProperties(ridge_pos.x + sample_dist, ridge_pos.z);
    auto [h_mx, n2] = terrain.GetPointProperties(ridge_pos.x - sample_dist, ridge_pos.z);
    auto [h_pz, n3] = terrain.GetPointProperties(ridge_pos.x, ridge_pos.z + sample_dist);
    auto [h_mz, n4] = terrain.GetPointProperties(ridge_pos.x, ridge_pos.z - sample_dist);

    grad_x = h_px - h_mx;
    grad_z = h_pz - h_mz;

    // Tunnel direction is perpendicular to the gradient (along the ridge)
    // Or if gradient is small, just use a default direction
    glm::vec2 tunnel_dir;
    float grad_len = std::sqrt(grad_x * grad_x + grad_z * grad_z);

    if (grad_len > 2.0f) {
        // Perpendicular to gradient - tunnel goes across the ridge
        tunnel_dir = glm::normalize(glm::vec2(-grad_z, grad_x));
    } else {
        // Default diagonal direction
        tunnel_dir = glm::normalize(glm::vec2(1.0f, 1.0f));
    }

    // Place tunnel endpoints on opposite sides of the ridge
    float tunnel_half_length = 25.0f;
    glm::vec3 offset(tunnel_dir.x * tunnel_half_length, 0, tunnel_dir.y * tunnel_half_length);

    glm::vec3 start_xz = ridge_pos - offset;
    glm::vec3 end_xz = ridge_pos + offset;

    // Get actual terrain heights at tunnel endpoints
    auto [start_h, sn] = terrain.GetPointProperties(start_xz.x, start_xz.z);
    auto [end_h, en] = terrain.GetPointProperties(end_xz.x, end_xz.z);

    result.start = glm::vec3(start_xz.x, start_h, start_xz.z);
    result.end = glm::vec3(end_xz.x, end_h, end_xz.z);
    result.found = true;

    return result;
}

int main() {
    Visualizer vis(1280, 720, "Cave & Tunnel Demo");

    // Install a terrain generator
    auto terrain = std::make_shared<TerrainGenerator>(42);
    vis.InstallTerrainGenerator(terrain);

    CaveGenerator cave_gen(12345);

    // 1. Create a cave with an opening in the terrain
    glm::vec3 cave_entrance(60, 0, 60);
    auto [th, tn] = terrain->GetPointProperties(cave_entrance.x, cave_entrance.z);
    cave_entrance.y = th;

    std::cout << "Cave entrance at: (" << cave_entrance.x << ", " << cave_entrance.y << ", " << cave_entrance.z << ")" << std::endl;

    // Cut a hole in the terrain for cave entrance
    vis.AddTerrainOpening(cave_entrance, 8.0f);

    // Generate the cave mesh
    std::cout << "Generating cave mesh..." << std::endl;
    auto cave_mesh = cave_gen.GenerateCaveMesh(cave_entrance, 70.0f, 0.75f);
    auto cave_entity = std::make_shared<Cave>(cave_mesh);
    vis.AddShape(cave_entity);

    // 2. Create a tunnel through a ridge
    std::cout << "Finding ridge for tunnel..." << std::endl;

    // Try several search locations to find a good ridge
    TunnelPath tunnel;
    std::vector<std::pair<float, float>> search_locations = {
        {-80, -80}, {100, -50}, {-50, 100}, {150, 50}, {-100, 50}
    };

    for (const auto& [sx, sz] : search_locations) {
        tunnel = FindTunnelThroughRidge(*terrain, sx, sz);
        if (tunnel.found && tunnel.ridge_center.y >= 12.0f) {
            std::cout << "Found ridge at height " << tunnel.ridge_center.y << std::endl;
            break;
        }
    }

    if (tunnel.found) {
        std::cout << "Tunnel through ridge at (" << tunnel.ridge_center.x << ", "
                  << tunnel.ridge_center.y << ", " << tunnel.ridge_center.z << ")" << std::endl;
        std::cout << "  Start: (" << tunnel.start.x << ", " << tunnel.start.y << ", " << tunnel.start.z << ")" << std::endl;
        std::cout << "  End: (" << tunnel.end.x << ", " << tunnel.end.y << ", " << tunnel.end.z << ")" << std::endl;

        // Cut holes at tunnel entrances
        vis.AddTerrainOpening(tunnel.start, 8.0f);
        vis.AddTerrainOpening(tunnel.end, 8.0f);

        // Generate tunnel mesh
        std::cout << "Generating tunnel mesh..." << std::endl;
        auto tunnel_mesh = cave_gen.GenerateTunnelMesh(tunnel.start, tunnel.end, 0.8f);
        auto tunnel_entity = std::make_shared<Cave>(tunnel_mesh);
        vis.AddShape(tunnel_entity);
    } else {
        std::cout << "No suitable ridge found for tunnel in this terrain." << std::endl;
    }

    // Setup camera to view the cave entrance
    auto& cam = vis.GetCamera();
    cam.x = cave_entrance.x + 40;
    cam.y = cave_entrance.y + 25;
    cam.z = cave_entrance.z + 40;
    cam.yaw = 225;
    cam.pitch = -25;
    cam.speed = 30.0f;

    std::cout << "\nControls:" << std::endl;
    std::cout << "  WASD - Move" << std::endl;
    std::cout << "  Mouse - Look" << std::endl;
    std::cout << "  Space/Shift - Up/Down" << std::endl;
    std::cout << "  ESC - Exit" << std::endl;
    std::cout << "\nFly into the cave entrance or tunnel openings to explore!" << std::endl;

    vis.Run();

    return 0;
}
