
#include "graphics.h"
#include "model.h"
#include "constants.h"
#include <iostream>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace Boidsish;

int main() {
    try {
        Visualizer visualizer(1024, 768, "Mesh Explosion Test");

        auto teapot = std::make_shared<Model>("assets/utah_teapot.obj");
        teapot->SetPosition(0.0f, 0.0f, 0.0f);
        teapot->SetScale(glm::vec3(0.6f)); // Doubled scale
        teapot->SetColor(0.8f, 0.4f, 0.1f, 1.0f); // Bronze/Orange color
        visualizer.AddShape(teapot);

        Camera& cam = visualizer.GetCamera();
        cam.x = 0.0f; cam.y = 1.0f; cam.z = 2.0f; // Moved camera closer
        cam.pitch = -10.0f; cam.yaw = -90.0f;

        Light sun;
        sun.position = glm::vec3(5.0f, 5.0f, 5.0f);
        sun.color = glm::vec3(1.0f, 1.0f, 1.0f);
        sun.intensity = 100.0f;
        sun.base_intensity = 100.0f;
        sun.type = 0;
        visualizer.GetLightManager().AddLight(sun);

        bool exploded = false;
        float timer = 0.0f;
        float explosion_time = 0.5f;

        visualizer.TogglePostProcessingEffect("Atmosphere", false);

        visualizer.AddShapeHandler([&](float dt) -> std::vector<std::shared_ptr<Shape>> {
            timer += dt;
            if (!exploded && timer >= explosion_time) {
                std::cout << "[INFO] Triggering explosion" << std::endl;
                visualizer.ExplodeShape(teapot, 2.0f);
                teapot->SetColor(0.8f, 0.4f, 0.1f, 0.0f); // Hide the teapot
                exploded = true;
            }
            return {};
        });

        visualizer.Run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
