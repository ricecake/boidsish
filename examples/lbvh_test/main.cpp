#include <iostream>
#include <vector>
#include <random>
#include <glm/glm.hpp>
#include "graphics.h"
#include "lbvh_manager.h"

using namespace Boidsish;

int main() {
    // 1. Initialize Visualizer (Headless or with dummy window)
    Visualizer visualizer(1280, 720, "LBVH Test");

    // 2. Setup LBVH
    LBVHManager lbvh;
    std::vector<LBVH_AABB> aabbs;
    std::vector<uint32_t> active;

    // Create some random AABBs
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-50.0f, 50.0f);
    std::uniform_real_distribution<float> size_dist(0.5f, 2.0f);

    for (int i = 0; i < 100; ++i) {
        glm::vec3 pos(dist(rng), dist(rng), dist(rng));
        float s = size_dist(rng);
        aabbs.push_back({glm::vec4(pos - glm::vec3(s), 0.0f), glm::vec4(pos + glm::vec3(s), 0.0f)});
        active.push_back(1);
    }

    std::cout << "Building LBVH for 100 objects..." << std::endl;
    lbvh.Build(aabbs, active, glm::vec3(-60.0f), glm::vec3(60.0f));
    std::cout << "LBVH Build complete." << std::endl;

    // 3. Move some objects and refit
    for (int i = 0; i < 10; ++i) {
        aabbs[i].min_pt += glm::vec4(0.1f, 0.1f, 0.1f, 0.0f);
        aabbs[i].max_pt += glm::vec4(0.1f, 0.1f, 0.1f, 0.0f);
    }

    std::cout << "Refitting LBVH..." << std::endl;
    lbvh.Refit(aabbs);
    std::cout << "Refit complete." << std::endl;

    std::cout << "LBVH Test PASSED (no crashes during GPU construction/refit)." << std::endl;

    return 0;
}
