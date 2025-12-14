#include <cmath>
#include <functional>
#include <iostream>
#include <random>

#include "graphics.h"
#include "spatial_entity_handler.h"

using namespace Boidsish;

namespace SimulationParameters {
    // World and Camera
    constexpr int   kScreenWidth = 1600;
    constexpr int   kScreenHeight = 1200;
    constexpr float kWorldBounds = 50.0f;

    // Entity Counts
    constexpr int kNumFlock = 50;
    constexpr int kNumPredators = 3;
    constexpr int kNumFood = 20;

    // Food
    constexpr float kFoodSize = 4.0f;
    constexpr float kFoodEnergy = 20.0f;

    // Creature
    constexpr float kInitialEnergy = 100.0f;
    constexpr float kEnergyCostFactor = 0.1f;

    // Flocking Entity
    constexpr float kFlockMaxSpeed = 5.0f;
    constexpr float kFlockBaselineSpeed = 1.0f;
    constexpr float kFlockSize = 6.0f;
    constexpr int   kFlockTrailLength = 40;
    constexpr float kFlockNeighborRadius = 10.0f;
    constexpr float kFlockFoodCompetitionBoost = 2.0f;
    constexpr float kFlockLowEnergyThreshold = 80.0f;
    constexpr float kFlockSeparationWeight = 1.5f;
    constexpr float kFlockAlignmentWeight = 1.0f;
    constexpr float kFlockCohesionWeight = 1.0f;
    constexpr float kFlockPredatorAvoidanceWeight = 2.0f;

    // Predator
    constexpr float kPredatorMaxSpeed = 8.0f;
    constexpr float kPredatorBaselineSpeed = 2.0f;
    constexpr float kPredatorSize = 10.0f;
    constexpr int   kPredatorTrailLength = 80;
    constexpr float kPredatorHuntRadius = 20.0f;

    // Interaction Radii
    constexpr float kFoodConsumptionRadius = 2.0f;
    constexpr float kPreyCaptureRadius = 2.0f;
}

// A stationary food source.
class FoodEntity : public Entity<> {
public:
    FoodEntity(int id) : Entity(id) {
        SetSize(SimulationParameters::kFoodSize);
        SetColor(0.1f, 0.8f, 0.1f); // Green
        SetTrailLength(0);
    }

    void UpdateEntity(EntityHandler& handler, float time, float delta_time) override {
        // Food doesn't do anything.
		(void)handler;
		(void)time;
		(void)delta_time;
    }
};

// Base class for all creatures in the simulation.
class CreatureEntity : public Entity<> {
public:
    CreatureEntity(int id, float max_speed, float baseline_speed)
        : Entity(id), energy_(SimulationParameters::kInitialEnergy), max_speed_(max_speed), baseline_speed_(baseline_speed) {}

    float GetEnergy() const { return energy_; }
    void AddEnergy(float amount) { energy_ += amount; }

protected:
    void ConsumeEnergy(float delta_time) {
        float speed = GetVelocity().Magnitude();
        float energy_cost = 0.0f;
        if (speed > baseline_speed_) {
            energy_cost = (speed - baseline_speed_) * SimulationParameters::kEnergyCostFactor;
        }
        energy_ -= energy_cost * delta_time;
    }

    float energy_;
    float max_speed_;
    float baseline_speed_;
};

class FlockingEntity; // Forward declaration

class PredatorEntity : public CreatureEntity {
public:
    PredatorEntity(int id) : CreatureEntity(id, SimulationParameters::kPredatorMaxSpeed, SimulationParameters::kPredatorBaselineSpeed) {
        SetSize(SimulationParameters::kPredatorSize);
        SetColor(0.9f, 0.1f, 0.1f); // Red
        SetTrailLength(SimulationParameters::kPredatorTrailLength);
    }

    void UpdateEntity(EntityHandler& handler, float time, float delta_time) override;

private:
    static std::mt19937 gen_;
    static std::uniform_real_distribution<float> dis_;
};

std::mt19937 PredatorEntity::gen_{std::random_device{}()};
std::uniform_real_distribution<float> PredatorEntity::dis_{-1.0f, 1.0f};

class FlockingEntity : public CreatureEntity {
public:
    FlockingEntity(int id) : CreatureEntity(id, SimulationParameters::kFlockMaxSpeed, SimulationParameters::kFlockBaselineSpeed) {
        SetSize(SimulationParameters::kFlockSize);
        SetColor(0.8f, 0.8f, 0.2f); // Yellow
        SetTrailLength(SimulationParameters::kFlockTrailLength);
    }

    void UpdateEntity(EntityHandler& handler, float time, float delta_time) override {
        auto& spatial_handler = static_cast<SpatialEntityHandler&>(handler);

        // Find neighbors
        auto neighbors = spatial_handler.GetEntitiesInRadius<EntityBase>(GetPosition(), SimulationParameters::kFlockNeighborRadius);

        Vector3 separation, alignment, cohesion, predator_avoidance, food_attraction;
        int flockmate_count = 0;

        std::shared_ptr<EntityBase> closest_food = nullptr;
        float min_food_dist = 1000.0f;

        for (auto& neighbor_ptr : neighbors) {
             if (neighbor_ptr.get() == this) continue;

            if (dynamic_cast<FlockingEntity*>(neighbor_ptr.get())) {
                separation += (GetPosition() - neighbor_ptr->GetPosition()).Normalized() / GetPosition().DistanceTo(neighbor_ptr->GetPosition());
                alignment += neighbor_ptr->GetVelocity();
                cohesion += neighbor_ptr->GetPosition();
                flockmate_count++;
            } else if (dynamic_cast<PredatorEntity*>(neighbor_ptr.get())) {
                predator_avoidance += (GetPosition() - neighbor_ptr->GetPosition()).Normalized() * (SimulationParameters::kFlockNeighborRadius / GetPosition().DistanceTo(neighbor_ptr->GetPosition()));
            } else if (dynamic_cast<FoodEntity*>(neighbor_ptr.get())) {
                float dist = GetPosition().DistanceTo(neighbor_ptr->GetPosition());
                if (dist < min_food_dist) {
                    min_food_dist = dist;
                    closest_food = neighbor_ptr;
                }
            }
        }

        if (flockmate_count > 0) {
            alignment /= flockmate_count;
            cohesion = (cohesion / flockmate_count - GetPosition()).Normalized();
        }

        float speed_boost = 0.0f;
        if (closest_food && energy_ < SimulationParameters::kFlockLowEnergyThreshold) {
            food_attraction = (closest_food->GetPosition() - GetPosition()).Normalized();

            // Check for competitors
            for (auto& neighbor_ptr : neighbors) {
                if (neighbor_ptr.get() != this && dynamic_cast<FlockingEntity*>(neighbor_ptr.get())) {
                    if (neighbor_ptr->GetPosition().DistanceTo(closest_food->GetPosition()) < min_food_dist) {
                        speed_boost = SimulationParameters::kFlockFoodCompetitionBoost;
                        break;
                    }
                }
            }
        }

        Vector3 steering = separation * SimulationParameters::kFlockSeparationWeight +
                           alignment * SimulationParameters::kFlockAlignmentWeight +
                           cohesion * SimulationParameters::kFlockCohesionWeight +
                           predator_avoidance * SimulationParameters::kFlockPredatorAvoidanceWeight +
                           food_attraction * (1.0f - energy_ / SimulationParameters::kInitialEnergy);

        Vector3 new_velocity = GetVelocity() + steering * delta_time;
        float current_max_speed = max_speed_ + speed_boost;
        if (new_velocity.Magnitude() > current_max_speed) {
            new_velocity = new_velocity.Normalized() * current_max_speed;
        }

        SetVelocity(new_velocity);
        ConsumeEnergy(delta_time);
		(void)time;
    }
};


void PredatorEntity::UpdateEntity(EntityHandler& handler, float time, float delta_time) {
    auto& spatial_handler = static_cast<SpatialEntityHandler&>(handler);

    auto closest_prey = spatial_handler.FindNearest<FlockingEntity>(GetPosition(), SimulationParameters::kPredatorHuntRadius);

    if (closest_prey) {
        float prey_dist = GetPosition().DistanceTo(closest_prey->GetPosition());
        Vector3 steering = (closest_prey->GetPosition() - GetPosition()).Normalized();

        float speed_factor = std::max(0.0f, 1.0f - (prey_dist / SimulationParameters::kWorldBounds));
        float desired_speed = baseline_speed_ + (max_speed_ - baseline_speed_) * speed_factor;

        Vector3 new_velocity = steering * desired_speed;
        SetVelocity(new_velocity);
    } else {
        // Wander
        Vector3 current_velocity = GetVelocity();
        if (current_velocity.MagnitudeSquared() < 0.1f) {
            current_velocity = Vector3(dis_(gen_), 0, dis_(gen_));
        }
        SetVelocity(current_velocity.Normalized() * baseline_speed_);
    }

    ConsumeEnergy(delta_time);
    (void)time;
}

class FlockingHandler : public SpatialEntityHandler {
public:
    FlockingHandler(int num_flock, int num_predators, int num_food) : gen_(rd_()), dis_(-SimulationParameters::kWorldBounds, SimulationParameters::kWorldBounds) {
        for (int i = 0; i < num_flock; ++i) {
            auto entity = std::make_shared<FlockingEntity>(next_id_++);
            entity->SetPosition(dis_(gen_), dis_(gen_) / 2.0f, dis_(gen_));
            AddEntity(entity->GetId(), entity);
        }

        for (int i = 0; i < num_predators; ++i) {
            auto entity = std::make_shared<PredatorEntity>(next_id_++);
            entity->SetPosition(dis_(gen_), dis_(gen_) / 2.0f, dis_(gen_));
            AddEntity(entity->GetId(), entity);
        }

        for (int i = 0; i < num_food; ++i) {
            SpawnFood();
        }
    }

protected:
    void PostTimestep(float time, float delta_time) override {
        // Handle interactions
        std::vector<int> entities_to_remove;
        for (auto const& [id, entity] : GetAllEntities()) {
            if (auto flock_member = std::dynamic_pointer_cast<FlockingEntity>(entity)) {
                // Check for food consumption
                 auto food = FindNearest<FoodEntity>(flock_member->GetPosition(), SimulationParameters::kFoodConsumptionRadius);
                 if (food) {
                    flock_member->AddEnergy(SimulationParameters::kFoodEnergy);
                    entities_to_remove.push_back(food->GetId());
                    SpawnFood();
                 }
            } else if (auto predator = std::dynamic_pointer_cast<PredatorEntity>(entity)) {
                // Check for prey capture
                auto prey = FindNearest<FlockingEntity>(predator->GetPosition(), SimulationParameters::kPreyCaptureRadius);
                if (prey) {
                    entities_to_remove.push_back(prey->GetId());
                }
            }
        }

        for (int id : entities_to_remove) {
            RemoveEntity(id);
        }
		(void)time;
		(void)delta_time;
    }

private:
    void SpawnFood() {
        auto entity = std::make_shared<FoodEntity>(next_id_++);
        entity->SetPosition(dis_(gen_), dis_(gen_) / 4.0f, dis_(gen_));
        AddEntity(entity->GetId(), entity);
    }
    int next_id_ = 0;
    std::random_device rd_;
    std::mt19937 gen_;
    std::uniform_real_distribution<> dis_;
};

int main() {
    try {
        Visualizer viz(SimulationParameters::kScreenWidth, SimulationParameters::kScreenHeight, "Boidsish - Flocking Demo");

        Camera camera(0.0f, 15.0f, 60.0f, -15.0f, 0.0f, 45.0f);
        viz.SetCamera(camera);

        FlockingHandler handler(SimulationParameters::kNumFlock, SimulationParameters::kNumPredators, SimulationParameters::kNumFood);
        viz.AddShapeHandler(std::ref(handler));

        std::cout << "Flocking Demo Started!" << std::endl;
        std::cout << "Controls:" << std::endl;
        std::cout << "  WASD - Move camera" << std::endl;
        std::cout << "  Space/Shift - Move up/down" << std::endl;
        std::cout << "  Mouse - Look around" << std::endl;
        std::cout << "  0 - Toggle auto-camera" << std::endl;
        std::cout << "  ESC - Exit" << std::endl;

        viz.Run();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}