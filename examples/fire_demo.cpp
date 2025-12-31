#include "graphics.h"
#include "dot.h"
#include <iostream>
#include <memory>
#include <vector>

int main() {
    try {
        Boidsish::Visualizer vis(1280, 720, "Fire Effect Demo");

        // Create a single fire effect and store its ID
        size_t fire_id = vis.AddFireEffect(glm::vec3(0.0f, 0.0f, 0.0f));

        // Create a dot that will move and act as the emitter
        auto moving_dot = std::make_shared<Boidsish::Dot>();

        // Use a shape handler to update the dot's and the fire's position each frame
        vis.AddShapeHandler([&](float time) {
            // Move the dot in a circle
            float x = 5.0f * cos(time * 0.5f);
            float z = 5.0f * sin(time * 0.5f);
            moving_dot->SetPosition(x, 1.0f, z); // Corrected method call

            // Update the fire effect's position to follow the dot
            vis.UpdateFireEffectPosition(fire_id, glm::vec3(x, 1.0f, z));

            // Return the dot to be rendered
            std::vector<std::shared_ptr<Boidsish::Shape>> shapes;
            shapes.push_back(moving_dot);
            return shapes;
        });

        vis.GetCamera().y = 10.0;
        vis.GetCamera().z = 20.0;
        vis.GetCamera().pitch = -30.0; // Angle the camera down
        vis.SetCameraMode(Boidsish::CameraMode::STATIONARY);

        vis.Run();
    } catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
