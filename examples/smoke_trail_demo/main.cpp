#include <iostream>
#include <vector>
#include <memory>
#include <cmath>

#include "graphics.h"
#include "fire_effect.h"
#include "shape.h"
#include "dot.h"
#include "logger.h"

int main() {
    try {
        Boidsish::Visualizer vis(1280, 720, "Voxel Smoke Trail Demo");

        // Set up camera
        vis.GetCamera().y = 30.0f;
        vis.GetCamera().z = 100.0f;
        vis.SetCameraMode(Boidsish::CameraMode::FREE);

        // Create a moving "emitter" sphere
        auto emitter_sphere = std::make_shared<Boidsish::Dot>(1, 0, 10.0f, 0, 2.0f, 0.8f, 0.8f, 0.9f);
        vis.AddShape(emitter_sphere);

        // Attach a smoke/vapor fire effect to the sphere
        auto smoke_trail = vis.AddFireEffect(
            glm::vec3(0, 10.0f, 0),
            Boidsish::FireEffectStyle::MissileExhaust, // Good for thick trails
            {0, 1.0f, 0},
            {0, 0, 0},
            5000 // Particle count
        );

        vis.AddShapeHandler([&](float time) {
            // Calculate a sweeping movement pattern
            float x = sin(time * 0.5f) * 40.0f;
            float z = cos(time * 0.3f) * 60.0f;
            float y = 15.0f + sin(time * 0.7f) * 10.0f;

            glm::vec3 pos(x, y, z);
            emitter_sphere->SetPosition(x, y, z);

            if (smoke_trail) {
                smoke_trail->SetPosition(pos);
                // Make the smoke "billow" slightly upward
                smoke_trail->SetDirection({0, 1.0f, 0});
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
