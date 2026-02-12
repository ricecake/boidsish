#include <iostream>
#include <vector>
#include <string>
#include "graphics.h"
#include "ConfigManager.h"

int main() {
    try {
        Boidsish::Visualizer vis(1280, 720, "SSSR Verification");

        // Ensure SSSR and Temporal Reprojection are enabled
        auto& config = Boidsish::ConfigManager::GetInstance();
        config.SetBool("sssr_enabled", true);
        config.SetBool("enable_temporal_reprojection", true);
        config.SetBool("enable_floor_reflection", false); // Disable legacy

        std::cout << "Running SSSR verification..." << std::endl;

        vis.Prepare();

        // Run for a few frames to let temporal accumulation kick in
        for(int i = 0; i < 60; ++i) {
            vis.Update();
            vis.Render();
        }

        std::cout << "SSSR verification completed." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
