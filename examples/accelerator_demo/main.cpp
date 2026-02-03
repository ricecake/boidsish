#include <iostream>
#include "graphics.h"
#include "fire_effect.h"

int main() {
    try {
        Boidsish::Visualizer vis(1280, 720, "Particle Accelerator Effect Demo");

        // Add the new Accelerator effect at the origin
        auto accelerator = vis.AddFireEffect(
            glm::vec3(0.0f, 5.0f, 0.0f),
            Boidsish::FireEffectStyle::Accelerator,
            glm::vec3(0.0f, 1.0f, 0.0f), // Initial direction (up)
            glm::vec3(0.0f),             // Initial velocity
            1000                         // Max particles
        );

        vis.GetCamera().y = 10.0f;
        vis.GetCamera().z = 30.0f;
        vis.SetCameraMode(Boidsish::CameraMode::STATIONARY);

        vis.Run();
    } catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
