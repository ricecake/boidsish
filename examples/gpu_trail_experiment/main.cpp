#include <iostream>
#include <memory>
#include <vector>
#include <cmath>

#include "graphics.h"
#include "logger.h"

using namespace Boidsish;

struct GpuBoid {
    glm::vec3 pos;
    glm::vec3 vel;
    glm::vec3 color;
    int trail_id;
};

int main() {
    try {
        Visualizer viz(1280, 720, "Gpu Trail SDF Experiment");

        std::vector<GpuBoid> boids;
        int num_boids = 20;

        for (int i = 0; i < num_boids; ++i) {
            GpuBoid b;
            b.pos = glm::vec3(
                (rand() % 200 - 100) / 5.0f,
                (rand() % 100 + 50) / 5.0f,
                (rand() % 200 - 100) / 5.0f
            );
            b.vel = glm::vec3(
                (rand() % 20 - 10) / 10.0f,
                (rand() % 20 - 10) / 10.0f,
                (rand() % 20 - 10) / 10.0f
            );
            b.color = glm::vec3(
                0.5f + (rand() % 50) / 100.0f,
                0.5f + (rand() % 50) / 100.0f,
                0.5f + (rand() % 50) / 100.0f
            );
            b.trail_id = viz.AddGpuTrail(256);
            boids.push_back(b);
        }

        viz.AddPrepareCallback([](Visualizer& v) {
            v.GetCamera().y = 25.0f;
            v.GetCamera().z = 40.0f;
            v.GetCamera().pitch = -30.0f;
        });

        float last_time = 0;

        viz.AddShapeHandler([&](float time) {
            float dt = time - last_time;
            if (dt <= 0) return std::vector<std::shared_ptr<Shape>>();
            last_time = time;

            for (auto& b : boids) {
                // Chaotic movement
                b.vel += glm::vec3(
                    sin(time * 2.0f + b.pos.z * 0.2f) * 0.05f,
                    cos(time * 3.0f + b.pos.x * 0.2f) * 0.05f,
                    sin(time * 1.5f + b.pos.y * 0.2f) * 0.05f
                );

                // Centering force
                b.vel -= b.pos * 0.01f;

                // Friction
                b.vel *= 0.99f;

                b.pos += b.vel * 20.0f * dt;

                viz.AddGpuTrailPoint(b.trail_id, b.pos, b.color, 0.2f);
            }

            return std::vector<std::shared_ptr<Shape>>();
        });

        viz.Run();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
