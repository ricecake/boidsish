#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <random>
#include <vector>

#include "graphics.h"
#include "model.h"
#include "spatial_entity_handler.h"
#include "terrain.h"
#include "field.h"
#include <glm/gtc/quaternion.hpp>

using namespace Boidsish;

// Forward declaration
class FlockingTerrainHandler;

class BirdEntity : public Entity<Model> {
public:
    BirdEntity(int id, const Vector3& start_pos);
    void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override;

private:
    Vector3 CalculateSeparation(const std::vector<std::shared_ptr<BirdEntity>>& neighbors);
    Vector3 CalculateAlignment(const std::vector<std::shared_ptr<BirdEntity>>& neighbors);
    Vector3 CalculateCohesion(const std::vector<std::shared_ptr<BirdEntity>>& neighbors);
};

// Need to define FlockingTerrainHandler before BirdEntity::UpdateEntity can use it
class FlockingTerrainHandler : public SpatialEntityHandler {
public:
    FlockingTerrainHandler(task_thread_pool::task_thread_pool& thread_pool, Visualizer& viz)
        : SpatialEntityHandler(thread_pool),
          viz_(viz),
          terrain_lut_(20.0f) { // Influence radius of 20.0f

        // Spawn birds at random positions over the terrain
        for (int i = 0; i < 100; ++i) {
            float x = (static_cast<float>(rand()) / RAND_MAX) * 200.0f - 100.0f;
            float z = (static_cast<float>(rand()) / RAND_MAX) * 200.0f - 100.0f;
            float y = std::get<0>(viz.GetTerrainPointProperties(x, z)) + 15.0f + (static_cast<float>(rand()) / RAND_MAX) * 10.0f;
            AddEntity<BirdEntity>(Vector3(x, y, z));
        }
    }

    void PreTimestep(float, float) override {
        // Cache the visible chunks for this frame
        visible_chunks_ = viz_.GetTerrainChunks();
    }

    const std::vector<std::shared_ptr<Terrain>>& GetVisibleChunks() const {
        return visible_chunks_;
    }

    const WendlandLUT& GetTerrainLut() const {
        return terrain_lut_;
    }

private:
    Visualizer& viz_;
    WendlandLUT terrain_lut_;
    std::vector<std::shared_ptr<Terrain>> visible_chunks_;
};


BirdEntity::BirdEntity(int id, const Vector3& start_pos) : Entity<Model>(id) {
    shape_ = std::make_shared<Model>("assets/bird.obj");
    SetPosition(start_pos);
    SetSize(0.1f);
    SetTrailLength(20);
    Vector3 startVel((rand() % 20 - 10), (rand() % 10 - 5), (rand() % 20 - 10));
    SetVelocity(startVel.Normalized() * 5.0f);
}

void BirdEntity::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
    auto& spatial_handler = static_cast<const SpatialEntityHandler&>(handler);
    auto position = GetPosition();

    auto neighbors = spatial_handler.GetEntitiesInRadius<BirdEntity>(position, 5.0f);

    Vector3 separation = CalculateSeparation(neighbors);
    Vector3 alignment = CalculateAlignment(neighbors);
    Vector3 cohesion = CalculateCohesion(neighbors);

    auto& flocking_handler = static_cast<const FlockingTerrainHandler&>(handler);
    auto visible_chunks = flocking_handler.GetVisibleChunks();
    const auto& terrain_lut = flocking_handler.GetTerrainLut();

    struct ForceAccumulator {
        glm::vec3 position;
        glm::vec3 forceAccumulator = {0.0f, 0.0f, 0.0f};
    } dummy_entity;
    dummy_entity.position = {position.x, position.y, position.z};

    for (const auto& chunk_ptr : visible_chunks) {
        if (chunk_ptr) {
            ApplyPatchInfluence(dummy_entity, *chunk_ptr, terrain_lut);
        }
    }

    Vector3 terrain_force(-dummy_entity.forceAccumulator.x, -dummy_entity.forceAccumulator.y, -dummy_entity.forceAccumulator.z);

    Vector3 total_force = separation * 2.5f + alignment * 1.0f + cohesion * 1.0f + terrain_force * 3.0f;

    auto current_velocity = GetVelocity();
    auto new_velocity = current_velocity + total_force * delta_time;

    float speed = new_velocity.Magnitude();
    if (speed > 8.0f) {
        new_velocity = new_velocity.Normalized() * 8.0f;
    } else if (speed < 3.0f) {
        new_velocity = new_velocity.Normalized() * 3.0f;
    }

    SetVelocity(new_velocity);

    if (new_velocity.MagnitudeSquared() > 0.001f) {
        glm::vec3 current_dir = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 target_dir = glm::normalize(glm::vec3(new_velocity.x, new_velocity.y, new_velocity.z));

        if (glm::dot(current_dir, target_dir) < -0.9999f) {
            GetShape()->SetRotation(glm::angleAxis(glm::pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f)));
        } else {
            glm::vec3 rotation_axis = glm::cross(current_dir, target_dir);
            float rotation_angle = acos(glm::dot(current_dir, target_dir));
            glm::quat rot = glm::angleAxis(rotation_angle, rotation_axis);
            GetShape()->SetRotation(rot);
        }
    }
}

Vector3 BirdEntity::CalculateSeparation(const std::vector<std::shared_ptr<BirdEntity>>& neighbors) {
    Vector3 separation = Vector3::Zero();
    Vector3 my_pos = GetPosition();
    int count = 0;
    float separation_radius = 2.0f;

    for (auto& neighbor : neighbors) {
        if (neighbor.get() != this) {
            Vector3 neighbor_pos = neighbor->GetPosition();
            float distance = my_pos.DistanceTo(neighbor_pos);

            if (distance < separation_radius && distance > 0) {
                Vector3 away = (my_pos - neighbor_pos).Normalized() / distance;
                separation += away;
                count++;
            }
        }
    }

    if (count > 0) {
        separation /= count;
    }
    return separation;
}

Vector3 BirdEntity::CalculateAlignment(const std::vector<std::shared_ptr<BirdEntity>>& neighbors) {
    Vector3 average_velocity = Vector3::Zero();
    int count = 0;
    for (auto& neighbor : neighbors) {
        if (neighbor.get() != this) {
            average_velocity += neighbor->GetVelocity();
            count++;
        }
    }

    if (count > 0) {
        average_velocity /= count;
        return (average_velocity - GetVelocity()).Normalized();
    }
    return Vector3::Zero();
}

Vector3 BirdEntity::CalculateCohesion(const std::vector<std::shared_ptr<BirdEntity>>& neighbors) {
    Vector3 center_of_mass = Vector3::Zero();
    int count = 0;
    for (auto& neighbor : neighbors) {
        if (neighbor.get() != this) {
            center_of_mass += neighbor->GetPosition();
            count++;
        }
    }

    if (count > 0) {
        center_of_mass /= count;
        return (center_of_mass - GetPosition()).Normalized();
    }
    return Vector3::Zero();
}

int main() {
    try {
        Visualizer viz(1280, 720, "Flocking Terrain Demo");

        Camera camera;
        camera.x = 0.0f;
        camera.y = 50.0f;
        camera.z = 50.0f;
        camera.pitch = -45.0f;
        camera.yaw = -90.0f;
        viz.SetCamera(camera);
        viz.SetCameraMode(CameraMode::AUTO);

        FlockingTerrainHandler handler(viz.GetThreadPool(), viz);
        viz.AddShapeHandler(std::ref(handler));

        viz.Run();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}