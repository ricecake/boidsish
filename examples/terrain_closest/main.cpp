#include <iostream>
#include <iomanip>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "graphics.h"
#include "terrain_generator.h"

void print_res(const std::string& label, float dist, glm::vec3 dir) {
    std::cout << "  " << std::left << std::setw(16) << label << ": Dist = "
              << std::fixed << std::setprecision(4) << dist
              << ", Dir = (" << dir.x << ", " << dir.y << ", " << dir.z << ")" << std::endl;
}

int main() {
    std::cout << "Starting Refined Closest Terrain Test..." << std::endl;

    Boidsish::TerrainGenerator terrain;

    // Test points
    std::vector<glm::vec3> test_points = {
        {0.0f, 100.0f, 0.0f},    // High above origin
        {10.0f, 0.1f, 10.0f},    // Just above surface
        {10.0f, -0.1f, 10.0f}    // Just below surface
    };

    for (const auto& p : test_points) {
        std::cout << "\nTesting Point: (" << p.x << ", " << p.y << ", " << p.z << ")" << std::endl;

        // 1. Spherical
        auto [s_dist, s_dir] = terrain.GetClosestTerrain(p);
        print_res("Spherical", s_dist, s_dir);

        // 2. Forward cone (narrow)
        glm::vec3 forward(0, 0, -1);
        auto [c1_dist, c1_dir] = terrain.GetClosestTerrain(p, 0.1f, forward);
        print_res("Cone (Narrow)", c1_dist, c1_dir);

        // 3. Forward cone (wide)
        auto [c2_dist, c2_dir] = terrain.GetClosestTerrain(p, 2.0f, forward);
        print_res("Cone (Wide)", c2_dist, c2_dir);
    }

    // Direction flip test
    std::cout << "\nDirection Flip Test (near surface):" << std::endl;
    float x = 50.0f, z = 50.0f;
    auto [h, norm] = terrain.GetTerrainPropertiesAtPoint(x, z);
    std::cout << "Terrain height at (" << x << ", " << z << ") is " << h << ", Normal: (" << norm.x << ", " << norm.y << ", " << norm.z << ")" << std::endl;

    glm::vec3 p_above(x, h + 0.01f, z);
    glm::vec3 p_below(x, h - 0.01f, z);

    auto [d_a, dir_a] = terrain.GetClosestTerrain(p_above);
    auto [d_b, dir_b] = terrain.GetClosestTerrain(p_below);

    print_res("Above", d_a, dir_a);
    print_res("Below", d_b, dir_b);

    std::cout << "Dot product of directions: " << glm::dot(dir_a, dir_b) << std::endl;
    if (glm::dot(dir_a, dir_b) < -0.9f) {
        std::cout << "Directions correctly point in opposite ways (both towards surface)." << std::endl;
    } else {
        std::cout << "Directions are NOT opposite. (May be expected if surface is very complex but usually they should be opposite for near-flat)." << std::endl;
    }

    std::cout << "\nTest Complete." << std::endl;
    return 0;
}
