#include <cmath>
#include <functional>
#include <iostream>
#include <random>
#include <vector>

#include "../include/boidsish.h"
#include "../include/collision.h"

using namespace Boidsish;

// A simple obstacle entity that doesn't do much
class ObstacleEntity : public Entity {
public:
    ObstacleEntity(int id, const Vector3& pos) : Entity(id) {
        SetPosition(pos);
        SetSize(1.0f);
        SetColor(0.7f, 0.7f, 0.7f); // Gray
        SetTrailLength(0);
    }

    void UpdateEntity(EntityHandler& handler, float time, float delta_time) override {
        (void)handler; (void)time; (void)delta_time;
        // This entity is static
    }
};

// An entity that casts a ray to detect obstacles
class RaycastEntity : public Entity {
public:
    RaycastEntity(int id, const Vector3& initial_pos, const Vector3& initial_vel)
        : Entity(id) {
        SetPosition(initial_pos);
        SetVelocity(initial_vel);
        SetSize(0.4f);
        SetTrailLength(40);
    }

    void UpdateEntity(EntityHandler& handler, float time, float delta_time) override {
        (void)time;
        (void)delta_time;

        // Wall bouncing logic
        Vector3 pos = GetPosition();
        Vector3 vel = GetVelocity();
        float bounds = 12.0f;
        if (pos.x < -bounds || pos.x > bounds) vel.x *= -1;
        if (pos.y < -bounds || pos.y > bounds) vel.y *= -1;
        if (pos.z < -bounds || pos.z > bounds) vel.z *= -1;
        SetVelocity(vel.Normalized() * 2.0f); // Maintain constant speed

        // Attempt to cast our handler to a CollisionHandler to access raycasting
        auto* collision_handler = dynamic_cast<CollisionHandler*>(&handler);
        if (!collision_handler) return;

        // Cast a ray in the direction of movement
        float ray_distance = 8.0f;
        auto hit = collision_handler->Raycast(GetPosition(), GetVelocity().Normalized(), ray_distance);

        bool did_hit = false;
        if (hit && hit->entity->GetId() != GetId()) {
            // We hit something that isn't ourselves!
            did_hit = true;
        }

        if (did_hit) {
            SetColor(1.0f, 0.4f, 0.4f); // Red on hit
            // Simple avoidance: turn away from the hit normal
            Vector3 new_vel = (GetVelocity() + hit->hit_normal * 1.5f).Normalized() * 2.0f;
            SetVelocity(new_vel);
        } else {
            SetColor(0.4f, 0.8f, 1.0f); // Blue otherwise
        }
    }
};

int main() {
    try {
        Visualizer viz(1600, 1200, "Boidsish - Raycasting Example");
        Camera camera(0.0f, 15.0f, 25.0f, -30.0f, 0.0f, 45.0f);
        viz.SetCamera(camera);

        CollisionHandler handler;

        // Random number generation
        std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<float> pos_dist(-10.0f, 10.0f);
        std::uniform_real_distribution<float> vel_dist(-1.0f, 1.0f);

        // Add static obstacles
        for (int i = 0; i < 15; ++i) {
            handler.AddEntity<ObstacleEntity>(Vector3(pos_dist(rng), pos_dist(rng), pos_dist(rng)));
        }

        // Add raycasting entities
        for (int i = 0; i < 10; ++i) {
            handler.AddEntity<RaycastEntity>(
                Vector3(pos_dist(rng), pos_dist(rng), pos_dist(rng)),
                Vector3(vel_dist(rng), vel_dist(rng), vel_dist(rng)).Normalized() * 2.0f
            );
        }

        viz.SetShapeHandler(std::ref(handler));

        std::cout << "Raycasting Example Started!" << std::endl;
        std::cout << "Blue entities are seeking. They will turn red and swerve upon detecting a gray obstacle in their path." << std::endl;

        viz.Run();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
