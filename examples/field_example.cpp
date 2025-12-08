#include "field_handler.h"
#include <iostream>

using namespace Boidsish;

// Emitter for a food source (attractor)
class FoodEmitter : public FieldEmitter {
public:
    FoodEmitter(const Vector3& pos, float strength) : pos_(pos), strength_(strength) {}

    Vector3 GetFieldContribution(const Vector3& position) const override {
        Vector3 diff = pos_ - position;
        float dist = diff.Magnitude();
        if (dist < 1.0f) return Vector3::Zero();
        return diff.Normalized() * (strength_ / (dist * dist));
    }

    AABB GetBoundingBox() const override {
        return AABB{pos_ - Vector3(10, 10, 10), pos_ + Vector3(10, 10, 10)};
    }
private:
    Vector3 pos_;
    float strength_;
};

// Emitter for a hazard (repulsor)
class HazardEmitter : public FieldEmitter {
public:
    HazardEmitter(const Vector3& pos, float strength) : pos_(pos), strength_(strength) {}

    Vector3 GetFieldContribution(const Vector3& position) const override {
        Vector3 diff = position - pos_;
        float dist = diff.Magnitude();
        if (dist > 5.0f || dist == 0) return Vector3::Zero();
        return diff.Normalized() * (strength_ / (dist * dist));
    }

    AABB GetBoundingBox() const override {
        return AABB{pos_ - Vector3(5, 5, 5), pos_ + Vector3(5, 5, 5)};
    }
private:
    Vector3 pos_;
    float strength_;
};

// Ant entity that follows food, avoids hazards, and lays pheromone trails
class AntEntity : public FieldEntity {
public:
    AntEntity(int id) : FieldEntity(id) {}

    void UpdateEntity(EntityHandler& handler, float time, float delta_time) override {
        (void)time;
        auto* field_handler = dynamic_cast<VectorFieldHandler*>(&handler);
        if (field_handler) {
            // Get forces from emitters
            Vector3 emitter_force = field_handler->GetFieldSumAt(position_);

            // Get forces from persistent field (pheromones)
            const auto& persistent_field = field_handler->GetPersistentField("pheromones");
            Vector3 pheromone_force = persistent_field.GetValue(
                static_cast<int>(position_.x),
                static_cast<int>(position_.y),
                static_cast<int>(position_.z)
            );

            // Combine forces
            Vector3 total_force = emitter_force + pheromone_force;

            // Update velocity and position
            velocity_ += total_force * delta_time;
            if (velocity_.Magnitude() > 10.0f) {
                velocity_ = velocity_.Normalized() * 10.0f;
            }
            position_ += velocity_ * delta_time;

            // Lay pheromone trail
            field_handler->AddToPersistentField("pheromones", position_, velocity_.Normalized() * 0.1f);
        }
    }
};

int main() {
    try {
        Visualizer viz(1024, 768, "Advanced Field Example");
        Camera camera(15.0f, 15.0f, 30.0f, -30.0f, -90.0f, 45.0f);
        viz.SetCamera(camera);

        auto handler = std::make_shared<VectorFieldHandler>(30, 30, 30);
        handler->CreateField("pheromones");

        // Add food and hazards
        handler->AddEmitter(std::make_shared<FoodEmitter>(Vector3(5, 15, 5), 100.0f));
        handler->AddEmitter(std::make_shared<HazardEmitter>(Vector3(15, 15, 15), 200.0f));
        handler->AddEmitter(std::make_shared<HazardEmitter>(Vector3(25, 15, 5), 200.0f));

        // Add ants
        for (int i = 0; i < 50; ++i) {
            handler->AddEntity<AntEntity>();
            auto entity = handler->GetEntity(i);
            entity->SetPosition(15.0f, 15.0f, 5.0f);
        }

        viz.SetDotHandler([handler](float time) {
            return (*handler)(time);
        });

        viz.Run();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
