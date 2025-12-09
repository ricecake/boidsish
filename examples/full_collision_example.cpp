#include <cmath>
#include <functional>
#include <iostream>
#include <random>
#include <vector>

#include "../include/boidsish.h"
#include "../include/collision.h"
#include "../include/graph.h"

using namespace Boidsish;

// An entity that moves and bounces off graph geometry
class BouncingEntity : public Entity {
public:
    BouncingEntity(int id, const Vector3& initial_pos, const Vector3& initial_vel)
        : Entity(id), collision_cooldown_(0.0f) {
        SetPosition(initial_pos);
        SetVelocity(initial_vel);
        SetSize(0.4f);
        SetTrailLength(30);
    }

    void UpdateEntity(EntityHandler& handler, float time, float delta_time) override {
        (void)handler; (void)time;

        // Apply some drag
        SetVelocity(GetVelocity() * 0.999f);

        // Wall bouncing
        Vector3 pos = GetPosition();
        Vector3 vel = GetVelocity();
        float bounds = 18.0f;
        if (pos.x < -bounds || pos.x > bounds) vel.x *= -1;
        if (pos.y < -bounds || pos.y > bounds) vel.y *= -1;
        if (pos.z < -bounds || pos.z > bounds) vel.z *= -1;
        SetVelocity(vel);

        // Cooldown timer for color change
        if (collision_cooldown_ > 0.0f) {
            collision_cooldown_ -= delta_time;
            float t = collision_cooldown_ / 0.5f; // Fade from red to blue
            SetColor(1.0f - t, 0.5f, t);
        } else {
            SetColor(0.2f, 0.5f, 1.0f); // Blue
        }
    }

    void OnCollision(Entity& other) override {
        if (collision_cooldown_ > 0.0f) return; // Avoid rapid re-collisions

        collision_cooldown_ = 0.5f;

        // Find the point of collision. For CCD, this is tricky. We'll approximate.
        // A real implementation would get the hit normal from the collision test.
        Vector3 normal = (GetPosition() - other.GetPosition()).Normalized();

        // Reflect velocity
        Vector3 new_vel = GetVelocity() - normal * 2.0f * GetVelocity().Dot(normal);
        SetVelocity(new_vel.Normalized() * 4.0f);
    }

private:
    float collision_cooldown_;
};


// Function to create the graph and add it to the collision system
std::shared_ptr<Graph> CreateAndRegisterGraph(CollisionHandler& handler) {
    auto graph = std::make_shared<Graph>();
    graph->vertices = {
        {{-8, -8, 0}, 1.5f, 1, 1, 1, 1},
        {{8, -8, 0}, 1.5f, 1, 1, 1, 1},
        {{8, 8, 0}, 1.5f, 1, 1, 1, 1},
        {{-8, 8, 0}, 1.5f, 1, 1, 1, 1},
         {{0, 0, 8}, 2.0f, 1, 1, 1, 1},
    };
    graph->edges = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {0, 4}, {1, 4}, {2, 4}, {3, 4}
    };

    handler.AddGraphForCollision(graph);
    return graph;
}

int main() {
    try {
        Visualizer viz(1600, 1200, "Boidsish - Full Collision Example");
        Camera camera(0.0f, 0.0f, 40.0f, 0.0f, 0.0f, 45.0f);
        viz.SetCamera(camera);

        CollisionHandler handler;
        auto graph = CreateAndRegisterGraph(handler);

        std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<float> pos_dist(-15.0f, 15.0f);
        std::uniform_real_distribution<float> vel_dist(-5.0f, 5.0f);

        for (int i = 0; i < 30; ++i) {
            handler.AddEntity<BouncingEntity>(
                Vector3(pos_dist(rng), pos_dist(rng), pos_dist(rng)),
                Vector3(vel_dist(rng), vel_dist(rng), vel_dist(rng))
            );
        }

        auto shape_function = [&](float time) {
            auto shapes = handler(time);
            shapes.push_back(graph); // Add the graph to be rendered
            return shapes;
        };

        viz.SetShapeHandler(shape_function);

        std::cout << "Full Collision Example Started!" << std::endl;
        std::cout << "Blue entities will bounce off both the nodes and edges of the graph." << std::endl;

        viz.Run();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
