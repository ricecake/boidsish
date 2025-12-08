#include <cmath>
#include <functional>
#include <iostream>
#include <random>
#include <vector>

#include "../include/boidsish.h"
#include "../include/collision.h"
#include "../include/graph.h"

using namespace Boidsish;

// An entity that uses raycasting to avoid a graph
class GraphAvoidingEntity : public Entity {
public:
    GraphAvoidingEntity(int id, const Vector3& initial_pos, const Vector3& initial_vel)
        : Entity(id) {
        SetPosition(initial_pos);
        SetVelocity(initial_vel);
        SetSize(0.3f);
        SetTrailLength(50);
    }

    void UpdateEntity(EntityHandler& handler, float time, float delta_time) override {
        (void)time;
        (void)delta_time;

        // Wall bouncing
        Vector3 pos = GetPosition();
        Vector3 vel = GetVelocity();
        float bounds = 15.0f;
        if (pos.x < -bounds || pos.x > bounds) vel.x *= -1;
        if (pos.y < -bounds || pos.y > bounds) vel.y *= -1;
        if (pos.z < -bounds || pos.z > bounds) vel.z *= -1;
        SetVelocity(vel.Normalized() * 3.0f);

        auto* collision_handler = dynamic_cast<CollisionHandler*>(&handler);
        if (!collision_handler) return;

        // Cast a ray to detect the graph
        float ray_distance = 6.0f;
        auto hit = collision_handler->Raycast(GetPosition(), GetVelocity().Normalized(), ray_distance);

        bool avoiding = false;
        if (hit) {
            // Check if the hit entity is part of the graph
            auto graph_vertex = std::dynamic_pointer_cast<GraphVertexEntity>(hit->entity);
            if (graph_vertex) {
                avoiding = true;
                // Avoid the graph vertex
                Vector3 new_vel = (GetVelocity() + hit->hit_normal * 2.0f).Normalized() * 3.0f;
                SetVelocity(new_vel);
            }
        }

        if (avoiding) {
            SetColor(1.0f, 0.6f, 0.2f); // Orange when avoiding
        } else {
            SetColor(0.2f, 1.0f, 0.6f); // Green otherwise
        }
    }
};

// Shape function to create and render the graph
std::shared_ptr<Graph> CreateAndRegisterGraph(CollisionHandler& handler) {
    auto graph = std::make_shared<Graph>();
    graph->vertices = {
        {{-5, 0, 0}, 1.0f, 1, 0, 0, 1},
        {{5, 0, 0}, 1.0f, 0, 1, 0, 1},
        {{0, 5, 0}, 1.0f, 0, 0, 1, 1},
        {{0, -5, 0}, 1.0f, 1, 1, 0, 1},
        {{0, 0, 5}, 1.0f, 0, 1, 1, 1},
        {{0, 0, -5}, 1.0f, 1, 0, 1, 1},
    };
    graph->edges = {
        {0, 2}, {1, 2}, {3, 0}, {3, 1},
        {4, 0}, {4, 1}, {4, 2}, {4, 3},
        {5, 0}, {5, 1}, {5, 2}, {5, 3},
    };

    handler.AddGraphForCollision(graph);
    return graph;
}

int main() {
    try {
        Visualizer viz(1600, 1200, "Boidsish - Graph Avoidance Example");
        Camera camera(0.0f, 0.0f, 30.0f, 0.0f, 0.0f, 45.0f);
        viz.SetCamera(camera);

        CollisionHandler handler;

        // Create the graph and register it for collision
        auto graph = CreateAndRegisterGraph(handler);

        // Random number generation
        std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<float> pos_dist(-12.0f, 12.0f);
        std::uniform_real_distribution<float> vel_dist(-1.0f, 1.0f);

        // Add avoiding entities
        for (int i = 0; i < 20; ++i) {
            handler.AddEntity<GraphAvoidingEntity>(
                Vector3(pos_dist(rng), pos_dist(rng), pos_dist(rng)),
                Vector3(vel_dist(rng), vel_dist(rng), vel_dist(rng)).Normalized() * 3.0f
            );
        }

        // The shape function now needs to return both the entities and the graph
        auto shape_function = [&](float time) {
            auto shapes = handler(time);
            shapes.push_back(graph);
            return shapes;
        };

        viz.SetShapeHandler(shape_function);

        std::cout << "Graph Avoidance Example Started!" << std::endl;
        std::cout << "Green entities will turn orange and swerve to avoid the graph nodes." << std::endl;

        viz.Run();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
