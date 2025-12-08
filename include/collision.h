#pragma once

#include "spatial_entity_handler.h"
#include "graph_entities.h"
#include <iostream>
#include <set>
#include <optional>
#include <limits>

namespace Boidsish {

// Structure to hold the result of a raycast
struct RaycastHit {
    std::shared_ptr<Entity> entity;
    Vector3 hit_point;
    Vector3 hit_normal;
    float distance;
};

class CollisionHandler : public SpatialEntityHandler {
public:
    constexpr static float kRaycastEpsilon = 0.001f;

    // Add a graph's vertices to the collision system as proxy entities
    void AddGraphForCollision(std::shared_ptr<Graph> graph) {
        for (const auto& vertex : graph->vertices) {
            AddEntity<GraphVertexEntity>(vertex, graph);
        }
    }

    // Raycast function
    std::optional<RaycastHit> Raycast(const Vector3& origin, const Vector3& direction, float max_distance) {
        std::vector<int> potential_hits;
        Vector3 end_point = origin + direction * max_distance;

        // Broad Phase: Use R-Tree to find potential hits that intersect the ray's AABB
        float min_b[3] = { std::min(origin.x, end_point.x), std::min(origin.y, end_point.y), std::min(origin.z, end_point.z) };
        float max_b[3] = { std::max(origin.x, end_point.x), std::max(origin.y, end_point.y), std::max(origin.z, end_point.z) };

        rtree_.Search(min_b, max_b, [&](int id) {
            potential_hits.push_back(id);
            return true; // continue searching
        });

        // Narrow Phase: Precise ray-sphere intersection test
        std::optional<RaycastHit> closest_hit;
        float min_distance = std::numeric_limits<float>::max();

        for (int id : potential_hits) {
            auto entity = GetEntity(id);
            if (!entity) continue;

            const Vector3& center = entity->GetPosition();
            float radius = entity->GetSize();

            Vector3 oc = origin - center;
            float a = direction.Dot(direction);
            float b = 2.0f * oc.Dot(direction);
            float c = oc.Dot(oc) - radius * radius;
            float discriminant = b * b - 4 * a * c;

            if (discriminant >= 0) {
                float t = (-b - std::sqrt(discriminant)) / (2.0f * a);
                if (t > kRaycastEpsilon && t < max_distance && t < min_distance) {
                    min_distance = t;
                    Vector3 hit_point = origin + direction * t;
                    closest_hit = RaycastHit{
                        entity,
                        hit_point,
                        (hit_point - center).Normalized(),
                        t
                    };
                }
            }
        }

        return closest_hit;
    }

private:
    // Swept-sphere vs swept-sphere collision test
    // Returns the time of impact (0-1) if a collision occurs, otherwise returns std::nullopt
    std::optional<float> SweptSphereSphere(const Entity& e1, const Entity& e2) {
        Vector3 p1_start = e1.GetPreviousPosition();
        Vector3 v1 = e1.GetPosition() - p1_start;

        Vector3 p2_start = e2.GetPreviousPosition();
        Vector3 v2 = e2.GetPosition() - p2_start;

        Vector3 relative_velocity = v1 - v2;
        Vector3 relative_start = p1_start - p2_start;
        float combined_radius = e1.GetSize() + e2.GetSize();

        float a = relative_velocity.Dot(relative_velocity);
        float b = 2 * relative_velocity.Dot(relative_start);
        float c = relative_start.Dot(relative_start) - combined_radius * combined_radius;

        // If the spheres are already overlapping at the start, we have a collision at time 0
        if (c < 0.0f) {
            return 0.0f;
        }

        // If 'a' is very close to zero, the entities are moving parallel or not at all relative to each other.
        // Since we've already checked for initial overlap (c < 0), they won't collide.
        if (std::abs(a) < 1e-6f) {
            return std::nullopt;
        }

        float discriminant = b * b - 4 * a * c;
        if (discriminant < 0) {
            return std::nullopt; // No real roots, no collision
        }

        float t = (-b - std::sqrt(discriminant)) / (2 * a);
        if (t >= 0.0f && t <= 1.0f) {
            return t; // Collision occurs within the frame
        }

        return std::nullopt;
    }

protected:
    void PostTimestep(float time, float delta_time) override {
        // First, update the R-tree with the new entity positions
        SpatialEntityHandler::PostTimestep(time, delta_time);

        // Keep track of pairs we've already checked
        std::set<std::pair<int, int>> checked_pairs;

        for (const auto& pair : GetAllEntities()) {
            auto& entity1 = pair.second;

            // Broad phase search box should encompass the entire movement volume
            Vector3 start_pos = entity1->GetPreviousPosition();
            Vector3 end_pos = entity1->GetPosition();
            float size = entity1->GetSize();

            float min_b[3] = {
                std::min(start_pos.x, end_pos.x) - size,
                std::min(start_pos.y, end_pos.y) - size,
                std::min(start_pos.z, end_pos.z) - size
            };
            float max_b[3] = {
                std::max(start_pos.x, end_pos.x) + size,
                std::max(start_pos.y, end_pos.y) + size,
                std::max(start_pos.z, end_pos.z) + size
            };

            rtree_.Search(min_b, max_b, [&](int id2) {
                if (entity1->GetId() == id2) return true;

                int id1 = entity1->GetId();
                if (id1 > id2) std::swap(id1, id2);
                if (checked_pairs.count({id1, id2})) return true;

                checked_pairs.insert({id1, id2});

                auto entity2 = GetEntity(id2);
                if (entity2) {
                    // Narrow phase: swept-sphere collision test
                    auto time_of_impact = SweptSphereSphere(*entity1, *entity2);
                    if (time_of_impact) {
                        entity1->OnCollision(*entity2);
                        entity2->OnCollision(*entity1);
                    }
                }
                return true;
            });
        }
    }
};

} // namespace Boidsish
