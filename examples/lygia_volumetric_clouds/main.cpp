#include <iostream>
#include <memory>
#include <vector>
#include <deque>
#include <cmath>
#include <algorithm>

#include "dot.h"
#include "entity.h"
#include "graphics.h"
#include "shape.h"
#include "sdf_volume_manager.h"
#include "light_manager.h"

using namespace Boidsish;

struct TrailSource {
    int id;
    float spawn_time;
    glm::vec3 pos;
};

int main() {
    try {
        Visualizer viz(1280, 720, "Lygia Volumetric Clouds Demo");

        // Set up a primary directional light (the "sun")
        auto& light_manager = viz.GetLightManager();
        light_manager.GetLights().clear();

        Light sun;
        sun.type = DIRECTIONAL_LIGHT;
        sun.direction = glm::normalize(glm::vec3(-1.0f, -1.0f, -0.5f));
        sun.color = glm::vec3(1.0f, 0.95f, 0.8f);
        sun.intensity = 2.0f;
        light_manager.AddLight(sun);

        // Add some ambient light
        light_manager.SetAmbientLight(glm::vec3(0.1f, 0.12f, 0.15f));

        std::deque<TrailSource> trail_sources;
        glm::vec3 rocket_pos(0.0f);

        viz.AddShapeHandler([&](float time) {
            // Update rocket position in a corkscrew pattern
            float speed = 1.2f;
            float radius = 12.0f;
            rocket_pos.x = std::cos(time * speed) * radius;
            rocket_pos.z = std::sin(time * speed) * radius;
            rocket_pos.y = 15.0f + std::sin(time * 0.4f) * 8.0f;

            // Spawn trail sources
            static float last_spawn_time = 0;
            if (time - last_spawn_time > 0.1f) {
                SdfSource source;
                source.position = rocket_pos;
                source.radius = 1.0f;
                // Initial fire color
                source.color = glm::vec3(1.0f, 0.5f, 0.1f);
                source.smoothness = 2.0f;
                source.charge = 1.0f;
                source.type = 0;

                int id = viz.AddSdfSource(source);
                trail_sources.push_back({id, time, rocket_pos});
                last_spawn_time = time;
            }

            // Update and prune sources
            float max_age = 6.0f;
            while (!trail_sources.empty() && time - trail_sources.front().spawn_time > max_age) {
                viz.RemoveSdfSource(trail_sources.front().id);
                trail_sources.pop_front();
            }

            for (auto& ts : trail_sources) {
                float age = time - ts.spawn_time;
                float life_pct = age / max_age;

                SdfSource source;
                // Drifting and rising
                source.position = ts.pos + glm::vec3(std::sin(age * 0.5f), age * 0.8f, std::cos(age * 0.5f));
                // Expanding over time
                source.radius = 1.5f + age * 1.8f;
                // Fade from fire to smoke/cloud
                source.color = glm::mix(glm::vec3(1.0f, 0.6f, 0.2f), glm::vec3(0.85f, 0.85f, 0.9f), std::min(1.0f, age * 1.5f));
                // Get smoother as it expands
                source.smoothness = 2.0f + age * 2.5f;
                source.charge = 1.0f;
                source.type = 0;

                viz.UpdateSdfSource(ts.id, source);
            }

            // Return a dot to represent the rocket
            auto rocket = std::make_shared<Dot>(0);
            rocket->SetPosition(rocket_pos.x, rocket_pos.y, rocket_pos.z);
            rocket->SetColor(1.0f, 0.1f, 0.05f);
            rocket->SetSize(0.6f);

            std::vector<std::shared_ptr<Shape>> shapes;
            shapes.push_back(rocket);
            return shapes;
        });

        // Set camera to watch the rocket
        viz.GetCamera().x = 0;
        viz.GetCamera().y = 20;
        viz.GetCamera().z = 50;
        viz.GetCamera().pitch = -15;

        viz.Run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
