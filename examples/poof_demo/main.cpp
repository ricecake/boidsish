#include <iostream>
#include "graphics.h"
#include "fire_effect.h"

int main() {
    try {
        Boidsish::Visualizer vis(1280, 720, "Poof Effect Demo");

        vis.GetCamera().y = 5.0;
        vis.GetCamera().z = 20.0;
        vis.SetCameraMode(Boidsish::CameraMode::STATIONARY);

        float lastPoofTime = 0;

        vis.AddShapeHandler([&](float time) {
            if ((time - lastPoofTime) > 4.5f) {
                vis.AddFireEffect(
                    glm::vec3(0.0f, 5.0f, 0.0f),
                    Boidsish::FireEffectStyle::Poof,
                    glm::vec3(0, 1, 0),
                    glm::vec3(0),
                    1000, // max particles
                    3.5f // lifetime
                );
                lastPoofTime = time;
            }
            return std::vector<std::shared_ptr<Boidsish::Shape>>{};
        });

        vis.Run();
    } catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
