#include <cmath>
#include <iostream>
#include <memory>
#include <random>
#include <vector>

#include "dot.h"
#include "entity.h"
#include "graphics.h"

// Helper to generate a random float between min and max
float rand_float(float min, float max) {
    static std::random_device              rd;
    static std::mt19937                    gen(rd());
    std::uniform_real_distribution<float> dis(min, max);
    return dis(gen);
}

class FireParticle: public Boidsish::Entity<Boidsish::Dot> {
public:
    // The entity constructor requires the ID as the first argument.
    FireParticle(int id, Boidsish::Vector3 position, Boidsish::Vector3 velocity, float lifetime)
        : Boidsish::Entity<Boidsish::Dot>(id)
        , initial_lifetime_(lifetime)
        , lifetime_(lifetime)
        , velocity_(velocity) {
        SetPosition(position);
        shape_ = std::make_shared<Boidsish::Dot>(id, position.x, position.y, position.z);
        shape_->SetColor(1.0f, 1.0f, 0.5f, 1.0f); // Bright yellow start
        shape_->SetSize(15.0f);
    }

    void UpdateEntity(const Boidsish::EntityHandler& handler, float time, float delta_time) override {
        // Apply some upward and outward force, simulating convection and a bit of wind
        velocity_.y += 2.0f * delta_time;
        velocity_.x += rand_float(-1.5f, 1.5f) * delta_time;
        velocity_.z += rand_float(-1.5f, 1.5f) * delta_time;

        // Update position based on velocity
        position_ += velocity_ * delta_time;

        // Decrease lifetime
        lifetime_ -= delta_time;

        // Update appearance based on age
        float life_ratio = std::max(0.0f, lifetime_ / initial_lifetime_);

        // Color transitions from yellow to red to dark
        float r = 1.0f * life_ratio + 0.8f * (1.0f - life_ratio);
        float g = 1.0f * life_ratio + 0.2f * (1.0f - life_ratio);
        float b = 0.5f * life_ratio;
        float a = life_ratio * life_ratio; // Fade out faster

        shape_->SetColor(r, g, b, a);
        shape_->SetSize(20.0f * life_ratio); // Shrink over time
    }

    bool IsAlive() const {
        return lifetime_ > 0.0f;
    }

private:
    float           initial_lifetime_;
    float           lifetime_;
    Boidsish::Vector3 velocity_;
};

class FireHandler: public Boidsish::EntityHandler {
public:
    // The base handler needs a thread pool at construction
    FireHandler(task_thread_pool::task_thread_pool& thread_pool, int particles_per_frame)
        : Boidsish::EntityHandler(thread_pool)
        , particles_per_frame_(particles_per_frame)
        , last_time_(0.0f) {}

    // The update function is driven by the visualizer's loop and receives time
    void Update(float time) {
        // The handler is responsible for calculating delta_time
        float delta_time = (last_time_ == 0.0f) ? 0.016f : time - last_time_;
        last_time_ = time;

        // Spawn new particles from the origin
        for (int i = 0; i < particles_per_frame_; ++i) {
            Boidsish::Vector3 position(0, 0, 0);
            Boidsish::Vector3 velocity(rand_float(-0.5f, 0.5f), rand_float(1.0f, 3.0f), rand_float(-0.5f, 0.5f));
            float             lifetime = rand_float(1.0f, 2.5f);
            // Use the templated AddEntity to construct particles in-place
            AddEntity<FireParticle>(position, velocity, lifetime);
        }

        // Update all particles
        for (auto& pair : GetAllEntities()) {
            pair.second->UpdateEntity(*this, time, delta_time);
        }

        // Remove dead particles
        std::vector<int> to_remove;
        for (const auto& pair : GetAllEntities()) {
            auto particle = std::dynamic_pointer_cast<FireParticle>(pair.second);
            if (particle && !particle->IsAlive()) {
                to_remove.push_back(pair.first);
            }
        }

        for (int id : to_remove) {
            QueueRemoveEntity(id);
        }
    }

    std::vector<std::shared_ptr<Boidsish::Shape>> GetShapes() {
        std::vector<std::shared_ptr<Boidsish::Shape>> shapes;
        // Iterate over the protected `entities_` map from the base class
        for (const auto& pair : GetAllEntities()) {
            pair.second->UpdateShape();
            shapes.push_back(pair.second->GetShape());
        }
        return shapes;
    }

private:
    int   particles_per_frame_;
    float last_time_;
};

int main() {
    try {
        Boidsish::Visualizer viz(1024, 768, "Boidsish - Fire Demo");

        Boidsish::Camera camera(0.0f, 2.0f, 10.0f, -10.0f, 0.0f, 45.0f);
        viz.SetCamera(camera);

        // The handler needs the visualizer's thread pool
        FireHandler fire_handler(viz.GetThreadPool(), 10);

        // The shape handler lambda must match the expected std::function signature
        viz.AddShapeHandler([&](float time) {
            fire_handler.Update(time);
            return fire_handler.GetShapes();
        });

        std::cout << "Starting Fire Demo..." << std::endl;
        viz.Run();
        std::cout << "Fire Demo ended." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
