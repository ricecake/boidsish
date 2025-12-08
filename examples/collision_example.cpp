#include <cmath>
#include <functional>
#include <iostream>
#include <random>

#include "../include/boidsish.h"
#include "../include/collision.h"

using namespace Boidsish;

// Example entity that moves and reacts to collisions
class BouncingEntity : public Entity {
public:
    BouncingEntity(int id, const Vector3& initial_pos, const Vector3& initial_vel)
        : Entity(id), collision_timer_(0.0f) {
        SetPosition(initial_pos);
        SetVelocity(initial_vel);
        SetSize(0.5f); // Smaller size for more interesting interactions
        SetTrailLength(20);
    }

    void UpdateEntity(EntityHandler& handler, float time, float delta_time) override {
        (void)handler;
        (void)time;

        // "Drag" to slow down over time
        SetVelocity(GetVelocity() * 0.995f);

        // Simple wall bouncing logic
        Vector3 pos = GetPosition();
        Vector3 vel = GetVelocity();
        float bounds = 10.0f;

        if (pos.x < -bounds || pos.x > bounds) vel.x *= -1;
        if (pos.y < -bounds || pos.y > bounds) vel.y *= -1;
        if (pos.z < -bounds || pos.z > bounds) vel.z *= -1;
        SetVelocity(vel);

        // Update color based on collision timer
        if (collision_timer_ > 0.0f) {
            SetColor(1.0f, 0.5f, 0.5f); // Reddish when in collision state
            collision_timer_ -= delta_time;
        } else {
            SetColor(0.8f, 0.8f, 1.0f); // Blueish normally
        }
    }

    void OnCollision(Entity& other) override {
        (void)other;
        collision_timer_ = 0.5f; // Set timer to show collision color for 0.5s

        // Simple collision response: reverse velocity
        SetVelocity(GetVelocity() * -1.0f);
    }

private:
    float collision_timer_;
};

int main() {
    try {
        Visualizer viz(1200, 800, "Boidsish - Collision Detection Example");
        Camera camera(0.0f, 0.0f, 25.0f, 0.0f, 0.0f, 45.0f);
        viz.SetCamera(camera);

        CollisionHandler handler;

        // Random number generation for entity properties
        std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<float> pos_dist(-8.0f, 8.0f);
        std::uniform_real_distribution<float> vel_dist(-2.0f, 2.0f);

        for (int i = 0; i < 50; ++i) {
            handler.AddEntity<BouncingEntity>(
                Vector3(pos_dist(rng), pos_dist(rng), pos_dist(rng)),
                Vector3(vel_dist(rng), vel_dist(rng), vel_dist(rng))
            );
        }

        viz.SetShapeHandler(std::ref(handler));

        std::cout << "Collision Detection Example Started!" << std::endl;
        std::cout << "Watch as entities bounce off walls and each other." << std::endl;
        std::cout << "Colliding entities will flash red." << std::endl;

        viz.Run();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
