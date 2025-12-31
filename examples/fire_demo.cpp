#include "graphics.h"
#include "dot.h"
#include <iostream>
#include <memory>
#include <vector>

int main() {
    try {
        Boidsish::Visualizer vis(1280, 720, "Fire Effect Demo");

        // Initial direction is upward, but it will be updated dynamically
        glm::vec3 initial_direction = glm::vec3(0.0f, 1.0f, 0.0f);
        size_t fire_id = vis.AddFireEffect(glm::vec3(0.0f, 0.0f, 0.0f), initial_direction);

        auto moving_dot = std::make_shared<Boidsish::Dot>();

        vis.AddShapeHandler([&](float time) {
            float speed = 1.0f;
            float radius = 5.0f;
            float x = radius * cos(time * speed);
            float z = radius * sin(time * speed);
            glm::vec3 current_pos(x, 1.0f, z);
            moving_dot->SetPosition(current_pos.x, current_pos.y, current_pos.z);

            vis.UpdateFireEffectPosition(fire_id, current_pos);

            // Calculate direction from the tangent of the circular path
            // The velocity vector is perpendicular to the position vector in a circle
            glm::vec3 direction = glm::normalize(glm::vec3(-z, 0.0f, x));
            vis.UpdateFireEffectDirection(fire_id, direction);

            std::vector<std::shared_ptr<Boidsish::Shape>> shapes;
            shapes.push_back(moving_dot);
            return shapes;
        });

        vis.GetCamera().y = 10.0;
        vis.GetCamera().z = 20.0;
        vis.GetCamera().pitch = -30.0;
        vis.SetCameraMode(Boidsish::CameraMode::STATIONARY);

        vis.Run();
    } catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
