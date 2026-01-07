#include <cmath>
#include <functional>
#include <iostream>
#include <random>

#include "entity.h"
#include "graphics.h"

using namespace Boidsish;

// Flocking entity with vortex-like behavior
class VortexFlockingEntity : public Entity<> {
public:
    VortexFlockingEntity(int id) : Entity(id) {
        SetSize(10.0f);
        SetTrailLength(50);
    }

    void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override {
        (void)time; // Mark unused
        (void)delta_time; // Mark unused

        const auto& entities = handler.GetAllEntities();
        if (entities.size() <= 1) return;

        // --- Parameters ---
        const Vector3 center_point(0.0f, 60.0f, 0.0f);
        const float cohesion_factor = 0.005f;
        const float separation_factor = 0.2f;
        const float separation_distance = 20.0f;
        const float vortex_strength = 0.5f;
        const float max_speed = 30.0f;
        const float terrain_avoidance_factor = 1.5f;
        const float terrain_avoidance_height = 25.0f;

        // --- Calculation ---
        Vector3 center_of_mass(0.0f, 0.0f, 0.0f);
        Vector3 separation_force(0.0f, 0.0f, 0.0f);
        int neighbor_count = 0;

        for (const auto& pair : entities) {
            if (pair.second.get() == this) continue;

            center_of_mass += pair.second->GetPosition();
            neighbor_count++;

            float dist = GetPosition().DistanceTo(pair.second->GetPosition());
            if (dist < separation_distance) {
                Vector3 away_vec = GetPosition() - pair.second->GetPosition();
                separation_force += away_vec / (dist * dist); // Inverse square
            }
        }

        if (neighbor_count > 0) {
            center_of_mass /= static_cast<float>(neighbor_count);
        }

        // 1. Cohesion
        Vector3 cohesion_vec = (center_of_mass - GetPosition()) * cohesion_factor;

        // 2. Separation
        Vector3 separation_vec = separation_force * separation_factor;

        // 3. Vortex Logic
        float dist_to_com = GetPosition().DistanceTo(center_of_mass);
        Vector3 to_center_xz = Vector3(center_point.x - GetPosition().x, 0, center_point.z - GetPosition().z);
        to_center_xz.Normalize();

        // Circular motion (tangent to the circle around the center point)
        Vector3 circular_motion = Vector3(to_center_xz.z, 0, -to_center_xz.x);

        // Spiral motion (inward and downward)
        Vector3 spiral_motion = Vector3(to_center_xz.x, -0.4f, to_center_xz.z);

        // Blend between circular and spiral based on distance from flock's center of mass
        float blend_factor = std::min(1.0f, dist_to_com / 80.0f); // 80 is the effective "radius" of the flock
        Vector3 vortex_vec = (circular_motion * (1.0f - blend_factor) + spiral_motion * blend_factor) * vortex_strength;

        // --- Combine and Apply Forces ---
        Vector3 new_velocity = GetVelocity() + cohesion_vec + separation_vec + vortex_vec;

        // 4. Terrain Avoidance
        auto terrain_props = handler.GetTerrainPointProperties(GetPosition().x, GetPosition().z);
        float height_above_terrain = GetPosition().y - std::get<0>(terrain_props);
        if (height_above_terrain < terrain_avoidance_height) {
            float avoidance_strength = (1.0f - (height_above_terrain / terrain_avoidance_height)) * terrain_avoidance_factor;
            new_velocity.y += avoidance_strength;
        }

        // --- Finalize ---
        // Limit speed
        if (new_velocity.MagnitudeSquared() > max_speed * max_speed) {
            new_velocity.Normalize();
            new_velocity *= max_speed;
        }

        SetVelocity(new_velocity);

        // Update color based on speed
        float speed = GetVelocity().Magnitude();
        float color_mix = std::min(1.0f, speed / max_speed);
        SetColor(0.2f + color_mix * 0.8f, 1.0f - color_mix, 0.8f, 1.0f);
    }
};

// Handler for managing the vortex flocking entities
class VortexFlockingHandler : public EntityHandler {
public:
    VortexFlockingHandler(task_thread_pool::task_thread_pool& thread_pool)
        : EntityHandler(thread_pool) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis_pos(-80.0, 80.0);
        std::uniform_real_distribution<> dis_y(30.0, 90.0);
        std::uniform_real_distribution<> dis_vel(-5.0, 5.0);

        for (int i = 0; i < 100; ++i) {
            auto entity = std::make_shared<VortexFlockingEntity>(i);
            entity->SetPosition(dis_pos(gen), dis_y(gen), dis_pos(gen));
            entity->SetVelocity(dis_vel(gen), dis_vel(gen), dis_vel(gen));
            AddEntity(i, entity);
        }
    }
};

int main() {
    try {
        // Create the visualizer as a shared_ptr to manage its lifetime
        auto viz = std::make_shared<Visualizer>(1200, 800, "Boidsish - Vortex Flocking Example");

        // Set up camera
        Camera camera(0.0f, 50.0f, 150.0f, -30.0f, -90.0f, 45.0f);
        viz->SetCamera(camera);

        // Create and set the entity handler
        VortexFlockingHandler handler(viz->GetThreadPool());
        handler.vis = viz; // Set the visualizer pointer to prevent segfault
        viz->AddShapeHandler(std::ref(handler));

        // Run the visualization
        viz->Run();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
