#include "entity.h"
#include "graphics.h"
#include "terrain_generator.h"
#include <iostream>
#include <memory>
#include <random>

using namespace Boidsish;

// An entity that moves forward and tries to avoid terrain.
class TerrainAvoidingEntity : public Entity<> {
public:
    TerrainAvoidingEntity(int id) : Entity(id) {
        SetSize(4.0f);
        SetTrailLength(20);
        SetColor(0.0f, 1.0f, 0.0f); // Green by default
    }

    void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override {
        (void)time;

        auto* vis = handler.GetVisualizer();
        if (!vis) return;

        TerrainGenerator* terrain_gen = vis->GetTerrainGenerator();
        if (!terrain_gen) return;

        const auto& boid_pos = GetPosition();
        const auto& boid_vel = GetVelocity();
        glm::vec3 pos(boid_pos.x, boid_pos.y, boid_pos.z);
        glm::vec3 vel(boid_vel.x, boid_vel.y, boid_vel.z);
        glm::vec3 forward_dir = glm::normalize(vel);

        float dist_to_terrain;
        bool collision_imminent = terrain_gen->Raycast(pos, forward_dir, 20.0f, dist_to_terrain);

        if (collision_imminent && dist_to_terrain < 20.0f) {
            SetColor(1.0f, 0.0f, 0.0f); // Red when avoiding

            // Simple avoidance: turn up
            glm::vec3 up_dir(0.0f, 1.0f, 0.0f);
            glm::vec3 new_vel = glm::normalize(vel + up_dir * 5.0f * (20.0f - dist_to_terrain) / 20.0f);
            SetVelocity(new_vel * 10.0f);
        } else {
            SetColor(0.0f, 1.0f, 0.0f); // Green otherwise
        }
    }
};

class AvoidanceHandler : public EntityHandler {
public:
    AvoidanceHandler(task_thread_pool::task_thread_pool& thread_pool, Visualizer* vis)
        : EntityHandler(thread_pool, vis) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> distrib(-50.0, 50.0);
        std::uniform_real_distribution<> y_distrib(10.0, 40.0);
        std::uniform_real_distribution<> vel_distrib(-5.0, 5.0);

        for (int i = 0; i < 20; ++i) {
            auto entity = std::make_shared<TerrainAvoidingEntity>(i);
            entity->SetPosition(distrib(gen), y_distrib(gen), distrib(gen));
            entity->SetVelocity(vel_distrib(gen), vel_distrib(gen), vel_distrib(gen));
            AddEntity(i, entity);
        }
    }
};

int main() {
    try {
        Visualizer viz(1280, 720, "Terrain Avoidance Example");

        Camera camera(0.0f, 30.0f, 80.0f, -20.0f, 0.0f, 0.0f);
        viz.SetCamera(camera);
        viz.SetCameraMode(CameraMode::FREE);

        AvoidanceHandler handler(viz.GetThreadPool(), &viz);
        viz.AddShapeHandler(std::ref(handler));

        std::cout << "Terrain Avoidance Example Started!" << std::endl;
        std::cout << "Entities will turn red and steer upwards to avoid terrain." << std::endl;

        viz.Run();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
