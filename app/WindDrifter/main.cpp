#include <iostream>
#include <memory>
#include <vector>
#include <random>
#include <functional>

#include "graphics.h"
#include "entity.h"
#include "polyhedron.h"
#include "wind_field.h"
#include "terrain_generator_interface.h"
#include "constants.h"
#include <GLFW/glfw3.h>

using namespace Boidsish;

// A simple drifter entity that follows the wind
class Drifter : public Entity<Polyhedron> {
public:
    Drifter(int id, std::shared_ptr<WindField> wind)
        : Entity<Polyhedron>(id, PolyhedronType::Dodecahedron), wind_(wind) {
        SetSize(4.0f);
        SetColor(0.2f, 0.7f, 1.0f, 0.8f);
        SetTrailLength(150);
        SetTrailIridescence(true);
        rigid_body_.mass_ = 1.0f;
        rigid_body_.linear_friction_ = 0.5f; // Very low friction to drift naturally
    }

    void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override {
        if (!wind_) return;

        glm::vec3 pos = GetPosition().Toglm();
        glm::vec3 wind_vel = wind_->Sample(pos, time);

        // Follow the wind field
        glm::vec3 current_vel = GetVelocity().Toglm();
        glm::vec3 force = (wind_vel - current_vel) * 2.0f;
        rigid_body_.AddForce(force);

        rigid_body_.Update(delta_time);
        rigid_body_.FaceVelocity();
        UpdateShape();
    }

private:
    std::shared_ptr<WindField> wind_;
};

// A wind visualization effect using many small drifting particles
class WindVisualizer : public EntityHandler {
public:
    WindVisualizer(task_thread_pool::task_thread_pool& pool, std::shared_ptr<WindField> wind, std::shared_ptr<EntityBase> target)
        : EntityHandler(pool), wind_(wind), target_(target) {}

    void Initialize(int count) {
        std::mt19937 gen(42);
        std::uniform_real_distribution<float> dist(-200.0f, 200.0f);

        for (int i = 0; i < count; ++i) {
            auto id = AddEntity<WindParticle>(wind_);
            auto particle = GetEntity(id);
            glm::vec3 target_pos = target_ ? target_->GetPosition().Toglm() : glm::vec3(0.0f);
            particle->SetPosition(target_pos.x + dist(gen), target_pos.y + dist(gen), target_pos.z + dist(gen));
        }
    }

    void PostTimestep(float time, float delta_time) override {
        if (!target_) return;

        glm::vec3 target_pos = target_->GetPosition().Toglm();
        std::mt19937 gen(static_cast<unsigned int>(time * 1000));
        std::uniform_real_distribution<float> dist(-150.0f, 150.0f);

        // Respawn particles that drift too far from the target
        for (auto& [id, entity] : GetAllEntities()) {
            glm::vec3 pos = entity->GetPosition().Toglm();
            if (glm::distance(pos, target_pos) > 250.0f) {
                // Move it to the "front" of the target in the direction of wind
                glm::vec3 wind_dir = glm::normalize(wind_->Sample(target_pos, time));
                glm::vec3 spawn_pos = target_pos + wind_dir * 150.0f + glm::vec3(dist(gen), dist(gen), dist(gen));
                entity->SetPosition(spawn_pos.x, spawn_pos.y, spawn_pos.z);
                entity->SetVelocity(0,0,0);
            }
        }
    }

private:
    class WindParticle : public Entity<Dot> {
    public:
        WindParticle(int id, std::shared_ptr<WindField> wind)
            : Entity<Dot>(id), wind_(wind) {
            SetSize(0.3f);
            SetColor(0.8f, 0.9f, 1.0f, 0.4f);
            SetTrailLength(30);
            rigid_body_.mass_ = 0.1f;
            rigid_body_.linear_friction_ = 1.0f;
        }

        void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override {
            glm::vec3 pos = GetPosition().Toglm();
            glm::vec3 wind_vel = wind_->Sample(pos, time);

            glm::vec3 current_vel = GetVelocity().Toglm();
            rigid_body_.AddForce((wind_vel - current_vel) * 5.0f);
            rigid_body_.Update(delta_time);
            UpdateShape();
        }
    private:
        std::shared_ptr<WindField> wind_;
    };

    std::shared_ptr<WindField> wind_;
    std::shared_ptr<EntityBase> target_;
};

int main() {
    try {
        auto visualizer = std::make_shared<Visualizer>(
            Constants::Project::Window::DefaultWidth(),
            Constants::Project::Window::DefaultHeight(),
            "WindDrifter"
        );

        auto terrain = visualizer->GetTerrain();
        terrain->SetWorldScale(4.0f);

        auto wind = std::make_shared<WindField>(terrain);
        wind->SetBaseSpeed(20.0f);

        auto handler = std::make_shared<EntityHandler>(visualizer->GetThreadPool());
        handler->SetVisualizer(visualizer);

        // Add the main drifter
        int drifter_id = handler->AddEntity<Drifter>(wind);
        auto drifter = handler->GetEntity(drifter_id);
        drifter->SetPosition(0.0f, 150.0f, 0.0f);

        visualizer->AddShapeHandler(std::ref(*handler));

        // Add many small particles to visualize the wind
        auto wind_viz = std::make_shared<WindVisualizer>(visualizer->GetThreadPool(), wind, drifter);
        wind_viz->SetVisualizer(visualizer);
        wind_viz->Initialize(300);
        visualizer->AddShapeHandler(std::ref(*wind_viz));

        visualizer->AddHudMessage("Wind Drifter - Adrift on the Curl", HudAlignment::TOP_CENTER, {0, 20}, 1.0f);

        // Setup chase camera
        visualizer->SetChaseCamera(drifter);
        auto& cam = visualizer->GetCamera();
        cam.follow_distance = 60.0f;
        cam.follow_elevation = 15.0f;
        cam.follow_look_ahead = 10.0f;

        visualizer->Run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
