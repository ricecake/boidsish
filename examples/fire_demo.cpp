#include "graphics.h"
#include <iostream>

int main() {
    try {
        Boidsish::Visualizer vis(1280, 720, "Fire Effect Demo");

        // Add a fire effect at the origin
        vis.AddFireEffect(glm::vec3(0.0f, 0.0f, 0.0f));

        // You can add more fire effects if you want
        // vis.AddFireEffect(glm::vec3(5.0f, 0.0f, 0.0f));
        // vis.AddFireEffect(glm::vec3(-5.0f, 0.0f, 0.0f));

        vis.GetCamera().y = 5.0;
        vis.GetCamera().z = 15.0;
        vis.SetCameraMode(Boidsish::CameraMode::STATIONARY);


        vis.Run();
    } catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
