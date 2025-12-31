#include "graphics.h"
#include "dot.h"
#include <iostream>
#include <memory>
#include <vector>

int main() {
    try {
        Boidsish::Visualizer vis(1280, 720, "Fire Effect Demo");

        // Initial direction is upward
        glm::vec3 initial_direction = glm::vec3(0.0f, 1.0f, 0.0f);
        size_t fire_id = vis.AddFireEffect(glm::vec3(0.0f, 0.0f, 0.0f), initial_direction);

        auto moving_dot = std::make_shared<Boidsish::Dot>();
        glm::vec3 last_pos(0.0f);

        vis.AddShapeHandler([&](float time) {
            float speed = 1.0f;
            float x = 5.0f * cos(time * speed);
            float z = 5.0f * sin(time * speed);
            glm::vec3 current_pos(x, 1.0f, z);
            moving_dot->SetPosition(current_pos.x, current_pos.y, current_pos.z);

            vis.UpdateFireEffectPosition(fire_id, current_pos);

            // Calculate direction from movement (tangent of the circular path)
            if (time > 0.0) {
                glm::vec3 direction = last_pos - current_pos;
                 if (glm::length(direction) > 0.001f) {
                    vis.UpdateFireEffectDirection(fire_id, glm::normalize(direction));
                }
            }
            last_pos = current_pos;

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
