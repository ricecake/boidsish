#include "FlockingEntity.h"
#include "VectorDemoEntity.h"
#include "FruitEntity.h"
#include "spatial_entity_handler.h"
#include "logger.h"

namespace Boidsish {

FlockingEntity::FlockingEntity(int id, const Vector3& start_pos) : Entity<>(id) {
    SetPosition(start_pos);
    SetSize(5.0f);
    SetTrailIridescence(true);
    SetTrailLength(25);
    Vector3 startVel((rand() % 30 - 15) * 2.0f, (rand() % 10 - 5) * 2.0f, (rand() % 16 - 8) * 2.0f);

    SetVelocity(startVel);
}

void FlockingEntity::UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
    (void)time;
    (void)delta_time; // Mark unused parameters

    auto& spatial_handler = static_cast<const SpatialEntityHandler&>(handler);
    auto  position = GetPosition();

    // Get neighbors and predators using spatial queries
    auto neighbors = spatial_handler.GetEntitiesInRadius<FlockingEntity>(position, 6.0f);
    auto predators = spatial_handler.GetEntitiesInRadius<VectorDemoEntity>(position, 2.0f);

    Vector3 pred;
    for (auto& a : predators) {
        auto pos = a->GetPosition();
        auto dis = position.DistanceTo(pos);
        auto dir = (position - pos).Normalized();
        pred += dir + (1 / (dis)*pos.Cross(GetPosition()).Normalized());
    }
    pred.Normalize();

    auto targetInstance = spatial_handler.FindNearest<FruitEntity>(position);
    if (!targetInstance) {
        return;
    }

    auto food = targetInstance->GetPosition();
    auto foodDistance = position.DistanceTo(food);

    if (foodDistance <= 0.6f) {
        SetVelocity(3 * (food - position));
        SetColor(1.0f, 0, 0, 1.0f);
        hunger_time -= targetInstance->GetValue() / 100 * hunger_time;
        hunger_time = std::max(0.0f, hunger_time);

        handler.QueueRemoveEntity(targetInstance->GetId());
        return;
    }

    auto distance = (foodDistance / 4 + hunger_time / 15 * (1 / std::min(1.0f, foodDistance / 5))) *
        (food - position).Normalized();

    Vector3 separation = CalculateSeparation(neighbors, predators);
    Vector3 alignment = CalculateAlignment(neighbors);
    Vector3 cohesion = CalculateCohesion(neighbors);
    Vector3 total_force = separation * 2.0f + alignment * 0.50f + cohesion * 1.30f + distance * 1.0f + pred * 2.0f;

    auto newVel = (GetVelocity() + total_force.Normalized()).Normalized();
    SetVelocity(newVel * 3);

    hunger_time += delta_time;
    hunger_time = std::min(100.0f, hunger_time);
    if (hunger_time < 5) {
        energy += delta_time;
    } else if (hunger_time > 15) {
        energy -= delta_time;
    }

    if (energy < 10) {
        logger::LOG("DEAD Flocker");
        handler.QueueRemoveEntity(GetId());
    } else if (energy >= 60) {
        energy -= 25;
        logger::LOG("New Flocker");
        handler.QueueAddEntity<FlockingEntity>(GetPosition());
    }

    // Color based on dominant behavior
    float sep_mag = separation.Magnitude();
    float align_mag = alignment.Magnitude();
    float coh_mag = cohesion.Magnitude();
    float dis_mag = distance.Magnitude();
    float pre_mag = pred.Magnitude();

    float b = (sep_mag + align_mag + coh_mag) / (sep_mag + align_mag + coh_mag + dis_mag + pre_mag + 0.1f);
    float g = dis_mag / (sep_mag + align_mag + coh_mag + dis_mag + pre_mag + 0.1f);
    float r = pre_mag / (sep_mag + align_mag + coh_mag + dis_mag + pre_mag + 0.1f);
    SetColor(r, g, b, 1.0f);
    SetTrailLength(energy);
}

Vector3 FlockingEntity::CalculateSeparation(
    const std::vector<std::shared_ptr<FlockingEntity>>&   neighbors,
    const std::vector<std::shared_ptr<VectorDemoEntity>>& predators
) {
    Vector3 separation = Vector3::Zero();
    Vector3 my_pos = GetPosition();
    int     count = 0;
    float   separation_radius = 2.50f;

    auto total_distance = 0.0f;
    for (auto& p : predators) {
        auto dist = p->GetPosition().DistanceTo(my_pos);
        if (dist <= 2) {
            total_distance += 1 / (dist * dist);
        }
    }
    separation_radius *= std::max(1.0f, total_distance);

    for (auto neighbor : neighbors) {
        if (neighbor.get() != this) {
            Vector3 neighbor_pos = neighbor->GetPosition();
            float   distance = my_pos.DistanceTo(neighbor_pos);

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

Vector3 FlockingEntity::CalculateAlignment(const std::vector<std::shared_ptr<FlockingEntity>>& neighbors) {
    Vector3 average_velocity = Vector3::Zero();
    int     count = 0;
    float   alignment_radius = 3.50f;

    Vector3 my_pos = GetPosition();
    for (auto neighbor : neighbors) {
        if (neighbor.get() != this) {
            Vector3 neighbor_pos = neighbor->GetPosition();
            float   distance = my_pos.DistanceTo(neighbor_pos);

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

Vector3 FlockingEntity::CalculateCohesion(const std::vector<std::shared_ptr<FlockingEntity>>& neighbors) {
    Vector3 center_of_mass = Vector3::Zero();
    int     count = 0;
    float   cohesion_radius = 6.0f;

    Vector3 my_pos = GetPosition();
    for (auto neighbor : neighbors) {
        if (neighbor.get() != this) {
            Vector3 neighbor_pos = neighbor->GetPosition();
            float   distance = my_pos.DistanceTo(neighbor_pos);

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

} // namespace Boidsish
