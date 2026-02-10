#include <iostream>
#include <vector>
#include <string>
#include "graphics.h"
#include "post_processing/PostProcessingManager.h"

// Define STB_IMAGE_IMPLEMENTATION once in the whole project if needed,
// but here it might conflict if it's already in the 'check' library.
// Since we are linking against 'check', let's assume it's there.

int main() {
    try {
        Visualizer vis("SSSR Verification", 1280, 720);

        // Ensure SSSR and Temporal Reprojection are enabled
        auto& config = Config::getInstance();
        config.set("enable_sssr", true);
        config.set("enable_temporal_reprojection", true);
        config.set("enable_floor_reflection", false); // Disable legacy

        std::cout << "Running SSSR verification..." << std::endl;

        // Run for a few frames to let temporal accumulation kick in
        for(int i = 0; i < 60; ++i) {
            vis.Render();
        }

        vis.TakeScreenshot("/tmp/verification_final_hiz.png");
        std::cout << "Screenshot saved to /tmp/verification_final_hiz.png" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
