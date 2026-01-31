#include "graphics.h"
#include "spatial_octree.h"
#include "constants.h"
#include "dot.h"
#include "shape.h"
#include <iostream>
#include <vector>
#include <random>
#include <algorithm>

using namespace Boidsish;

/**
 * @brief Custom shape to visualize the SpatialOctree.
 * It traverses the octree and renders a sphere for each leaf with density.
 */
class OctreeVisualizer : public Shape {
    const SpatialOctree& octree_;
public:
    OctreeVisualizer(const SpatialOctree& o) : octree_(o) {}

    void render() const override {
        octree_.Traverse([&](const glm::vec3& min, const glm::vec3& max, float density) {
            if (density < 0.005f) return;
            glm::vec3 center = (min + max) * 0.5f;
            glm::vec3 scale = (max - min) * 0.5f;

            // Map density to color: from light blue to white
            float intensity = std::min(1.0f, density * 2.0f);
            glm::vec3 color = glm::mix(glm::vec3(0.4f, 0.4f, 1.0f), glm::vec3(1.0f, 1.0f, 1.0f), intensity);

            Shape::RenderSphere(center, color, scale, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        });
    }

    void render(Shader& /* shader */, const glm::mat4& /* model_matrix */) const override {
        render();
    }

    glm::mat4 GetModelMatrix() const override { return glm::mat4(1.0f); }
    std::string GetInstanceKey() const override { return "OctreeVisualizer"; }
};

/**
 * @brief Entity that moves through space and leaves a density trail in the octree.
 */
class TrailEntity {
public:
    std::shared_ptr<Dot> dot;
    glm::vec3 vel;

    TrailEntity(int id, glm::vec3 p, glm::vec3 v) : vel(v) {
        dot = std::make_shared<Dot>(id, p.x, p.y, p.z, 1.0f, 1.0f, 0.3f, 0.3f);
    }

    void Update(float dt, SpatialOctree& octree) {
        glm::vec3 pos(dot->GetX(), dot->GetY(), dot->GetZ());
        pos += vel * dt;

        float half_size = octree.GetSize() * 0.45f;
        glm::vec3 center = octree.GetCenter();

        if (pos.x < center.x - half_size || pos.x > center.x + half_size) vel.x *= -1.0f;
        if (pos.y < center.y - half_size || pos.y > center.y + half_size) vel.y *= -1.0f;
        if (pos.z < center.z - half_size || pos.z > center.z + half_size) vel.z *= -1.0f;

        dot->SetPosition(pos.x, pos.y, pos.z);
        octree.AddDensity(pos, 15.0f * dt);
    }
};

/**
 * @brief Entity that follows the density gradient of the octree (ant-like behavior).
 */
class FollowingEntity {
public:
    std::shared_ptr<Dot> dot;
    glm::vec3 vel;

    FollowingEntity(int id, glm::vec3 p) : vel(0.0f) {
        dot = std::make_shared<Dot>(id, p.x, p.y, p.z, 0.5f, 0.3f, 1.0f, 0.3f);
    }

    void Update(float dt, const SpatialOctree& octree) {
        glm::vec3 pos(dot->GetX(), dot->GetY(), dot->GetZ());
        glm::vec3 grad = octree.GetGradient(pos);

        if (glm::length(grad) > 0.005f) {
            glm::vec3 target_vel = glm::normalize(grad) * 8.0f;
            vel = glm::mix(vel, target_vel, 3.0f * dt);
        } else {
            // Random wander if no trail is found
            vel += glm::vec3(
                ((rand() % 100) / 50.0f - 1.0f),
                ((rand() % 100) / 50.0f - 1.0f),
                ((rand() % 100) / 50.0f - 1.0f)
            ) * 1.0f;
            if (glm::length(vel) > 5.0f) vel = glm::normalize(vel) * 5.0f;
        }

        pos += vel * dt;
        dot->SetPosition(pos.x, pos.y, pos.z);
    }
};

int main() {
    Visualizer vis(1280, 720, "Spatial Octree Trail Demo");

    float octree_size = Constants::Class::SpatialOctree::DefaultSize();
    int octree_depth = Constants::Class::SpatialOctree::DefaultMaxDepth();
    SpatialOctree octree(glm::vec3(0, 20, 0), octree_size, octree_depth);

    std::vector<std::unique_ptr<TrailEntity>> trail_entities;
    trail_entities.push_back(std::make_unique<TrailEntity>(1, glm::vec3(0, 20, 0), glm::vec3(15, 8, 12)));

    std::vector<std::unique_ptr<FollowingEntity>> followers;
    for(int i = 0; i < 30; ++i) {
        glm::vec3 start_pos(
            (rand() % 40) - 20,
            20 + (rand() % 20) - 10,
            (rand() % 40) - 20
        );
        followers.push_back(std::make_unique<FollowingEntity>(100 + i, start_pos));
    }

    // Add the octree visualizer as a persistent shape
    auto octree_vis = std::make_shared<OctreeVisualizer>(octree);
    vis.AddShape(octree_vis);

    // Add entities
    for(auto& e : trail_entities) vis.AddShape(e->dot);
    for(auto& e : followers) vis.AddShape(e->dot);

    vis.AddPrepareCallback([&](Visualizer& v) {
        v.GetCamera().z = 100.0f;
        v.GetCamera().y = 30.0f;
        v.GetCamera().pitch = -15.0f;
        v.SetCameraMode(CameraMode::FREE);
    });

    vis.AddShapeHandler([&](float time) {
        static float last_time = time;
        float dt = std::max(0.001f, time - last_time);
        last_time = time;

        // Use physics parameters from constants
        float diff = Constants::Class::SpatialOctree::DefaultDiffusionRate();
        float decay = Constants::Class::SpatialOctree::DefaultDecayRate();
        glm::vec3 drift(1.0f, -0.2f, 0.5f); // Simulate gentle wind

        // Update octree physics
        octree.Update(dt, diff, decay, drift);

        // Update entities
        for(auto& e : trail_entities) e->Update(dt, octree);
        for(auto& e : followers) e->Update(dt, octree);

        return std::vector<std::shared_ptr<Shape>>();
    });

    vis.Run();
    return 0;
}
