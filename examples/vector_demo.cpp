#include "../include/boidsish.h"
#include <cmath>
#include <iostream>
#include <functional>

using namespace Boidsish;

// Example entity that demonstrates Vector3 operations
class VectorDemoEntity : public Entity {
public:
    VectorDemoEntity(int id, const Vector3& start_pos, const Vector3& target)
        : Entity(id), target_(target), start_pos_(start_pos), phase_(0.0f) {
        SetPosition(start_pos);
        SetSize(8.0f);
        SetTrailLength(60);
    }

    void UpdateEntity(EntityHandler& handler, float time, float delta_time) override {
        (void)handler; (void)time; // Mark unused parameters
        phase_ += delta_time;

        // Demonstrate various Vector3 operations
        Vector3 current_pos = GetPosition();
        Vector3 to_target = target_ - current_pos;

        float distance_to_target = to_target.Magnitude();

        if (distance_to_target > 0.1f) {
            // Normalize direction and move toward target
            Vector3 direction = to_target.Normalized();

            // Add some orbital motion using cross product
            Vector3 up = Vector3::Up();
            Vector3 tangent = direction.Cross(up).Normalized();

            // Combine linear movement with orbital motion
            Vector3 linear_vel = direction * 2.0f;
            Vector3 orbital_vel = tangent * sin(phase_ * 3.0f) * 1.5f;
            Vector3 total_velocity = linear_vel + orbital_vel;

            SetVelocity(total_velocity);
        } else {
            // Reached target, pick a new random target
            target_ = Vector3(
                (rand() % 20 - 10) * 0.5f,
                (rand() % 20 - 10) * 0.5f,
                (rand() % 20 - 10) * 0.5f
            );
        }

        // Color based on velocity magnitude and direction
        Vector3 vel = GetVelocity();
        float speed = vel.Magnitude();
        Vector3 vel_normalized = vel.Normalized();

        float r = 0.5f + 0.5f * std::abs(vel_normalized.x);
        float g = 0.5f + 0.5f * std::abs(vel_normalized.y);
        float b = 0.5f + 0.3f * (speed / 5.0f); // Blue based on speed
        SetColor(r, g, b, 1.0f);
    }

private:
    Vector3 target_;
    Vector3 start_pos_;
    float phase_;
};

// Example of using vector operations for flocking behavior
class FlockingEntity : public Entity {
public:
    FlockingEntity(int id, const Vector3& start_pos) : Entity(id) {
        SetPosition(start_pos);
        SetSize(6.0f);
        SetTrailLength(40);
    }

    void UpdateEntity(EntityHandler& handler, float time, float delta_time) override {
        (void)time; (void)delta_time; // Mark unused parameters

        // Get all flocking entities from the handler
        std::vector<FlockingEntity*> neighbors = handler.GetEntitiesByType<FlockingEntity>();

        Vector3 separation = CalculateSeparation(neighbors);
        Vector3 alignment = CalculateAlignment(neighbors);
        Vector3 cohesion = CalculateCohesion(neighbors);

        // Weight the flocking behaviors
        Vector3 total_force = separation * 2.0f + alignment * 1.0f + cohesion * 1.0f;

        // Add some random movement
        Vector3 random_force(
            (rand() % 200 - 100) / 100.0f,
            (rand() % 200 - 100) / 100.0f,
            (rand() % 200 - 100) / 100.0f
        );
        total_force += random_force * 0.3f;

        SetVelocity(total_force);

        // Color based on dominant behavior
        float sep_mag = separation.Magnitude();
        float align_mag = alignment.Magnitude();
        float coh_mag = cohesion.Magnitude();

        float r = sep_mag / (sep_mag + align_mag + coh_mag + 0.1f);
        float g = align_mag / (sep_mag + align_mag + coh_mag + 0.1f);
        float b = coh_mag / (sep_mag + align_mag + coh_mag + 0.1f);
        SetColor(r, g, b, 1.0f);
    }

private:

    Vector3 CalculateSeparation(const std::vector<FlockingEntity*>& neighbors) {
        Vector3 separation = Vector3::Zero();
        int count = 0;
        float separation_radius = 3.0f;

        Vector3 my_pos = GetPosition();
        for (auto* neighbor : neighbors) {
            if (neighbor != this) {
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

    Vector3 CalculateAlignment(const std::vector<FlockingEntity*>& neighbors) {
        Vector3 average_velocity = Vector3::Zero();
        int count = 0;
        float alignment_radius = 5.0f;

        Vector3 my_pos = GetPosition();
        for (auto* neighbor : neighbors) {
            if (neighbor != this) {
                Vector3 neighbor_pos = neighbor->GetPosition();
                float distance = my_pos.DistanceTo(neighbor_pos);

                if (distance < alignment_radius) {
                    average_velocity += neighbor->GetVelocity();
                    count++;
                }
            }
        }

        if (count > 0) {
            average_velocity /= count;
            return average_velocity.Normalized();
        }
        return Vector3::Zero();
    }

    Vector3 CalculateCohesion(const std::vector<FlockingEntity*>& neighbors) {
        Vector3 center_of_mass = Vector3::Zero();
        int count = 0;
        float cohesion_radius = 6.0f;

        Vector3 my_pos = GetPosition();
        for (auto* neighbor : neighbors) {
            if (neighbor != this) {
                Vector3 neighbor_pos = neighbor->GetPosition();
                float distance = my_pos.DistanceTo(neighbor_pos);

                if (distance < cohesion_radius) {
                    center_of_mass += neighbor_pos;
                    count++;
                }
            }
        }

        if (count > 0) {
            center_of_mass /= count;
            return (center_of_mass - my_pos).Normalized() * 0.5f;
        }
        return Vector3::Zero();
    }
};

// Handler for vector demonstration
class VectorDemoHandler : public EntityHandler {
public:
    VectorDemoHandler() {
        std::cout << "=== Vector3 Operations Demo ===" << std::endl;

        // Create some vector demo entities
        for (int i = 0; i < 5; i++) {
            Vector3 start_pos(
                (i - 2) * 3.0f,
                0.0f,
                0.0f
            );
            Vector3 target(
                (rand() % 10 - 5) * 2.0f,
                (rand() % 10 - 5) * 2.0f,
                (rand() % 10 - 5) * 2.0f
            );
            AddEntity<VectorDemoEntity>(start_pos, target);
        }

        // Create a flock of entities
        for (int i = 0; i < 8; i++) {
            Vector3 start_pos(
                (rand() % 10 - 5) * 2.0f,
                (rand() % 6 - 3) * 2.0f,
                (rand() % 10 - 5) * 2.0f
            );
            AddEntity<FlockingEntity>(start_pos);
        }

        std::cout << "Created 8 flocking entities and 5 target-seeking entities" << std::endl;
        std::cout << "Demonstrating Vector3 operations: addition, subtraction, normalization," << std::endl;
        std::cout << "dot product, cross product, magnitude, and distance calculations!" << std::endl;
        std::cout << "Flocking entities now automatically discover each other through the handler!" << std::endl;
    }
};

int main() {
    try {
        Boidsish::Visualizer viz(1200, 800, "Vector3 Operations Demo");

        // Set up camera
        Camera camera;
        camera.x = 0.0f; camera.y = 5.0f; camera.z = 15.0f;
        camera.yaw = -90.0f; camera.pitch = -15.0f;
        viz.SetCamera(camera);

        // Create and set the vector demo handler
        VectorDemoHandler handler;
        viz.SetDotHandler(std::ref(handler));

        std::cout << "Vector Demo Started!" << std::endl;
        std::cout << "Controls:" << std::endl;
        std::cout << "  WASD - Move camera" << std::endl;
        std::cout << "  Space/Shift - Move up/down" << std::endl;
        std::cout << "  Mouse - Look around" << std::endl;
        std::cout << "  0 - Toggle auto-camera" << std::endl;
        std::cout << "  ESC - Exit" << std::endl;

        // Main loop
        while (!viz.ShouldClose()) {
            viz.Update();
            viz.Render();
        }

        std::cout << "Vector demo ended." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}