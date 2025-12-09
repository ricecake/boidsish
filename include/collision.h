#pragma once

#include "spatial_entity_handler.h"
#include "graph_entities.h"
#include "collision_shapes.h"
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

    // Add a graph's vertices and edges to the collision system as proxy entities
    void AddGraphForCollision(std::shared_ptr<Graph> graph) {
        for (const auto& vertex : graph->vertices) {
            AddEntity<GraphVertexEntity>(vertex, graph);
        }
        for (const auto& edge : graph->edges) {
            AddEntity<GraphEdgeEntity>(graph->vertices[edge.vertex1_idx], graph->vertices[edge.vertex2_idx], graph);
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

        // Narrow Phase: Dispatch to the correct intersection test
        std::optional<RaycastHit> closest_hit;
        float min_distance = std::numeric_limits<float>::max();

        for (int id : potential_hits) {
            auto entity = GetEntity(id);
            if (!entity) continue;

            std::optional<float> hit_time;
            if (entity->GetCollisionShapeType() == CollisionShapeType::SPHERE) {
                 const Vector3& center = entity->GetPosition();
                float radius = entity->GetSize();
                Vector3 oc = origin - center;
                float a = direction.Dot(direction);
                float b = 2.0f * oc.Dot(direction);
                float c = oc.Dot(oc) - radius * radius;
                float discriminant = b * b - 4 * a * c;
                if (discriminant >= 0) {
                    hit_time = (-b - std::sqrt(discriminant)) / (2.0f * a);
                }
            } else if (entity->GetCollisionShapeType() == CollisionShapeType::CAPSULE) {
                auto capsule_entity = std::dynamic_pointer_cast<GraphEdgeEntity>(entity);
                if (capsule_entity) {
                    hit_time = RayCapsuleIntersect(origin, direction, capsule_entity->GetCapsule());
                }
            }

            if (hit_time && *hit_time > kRaycastEpsilon && *hit_time < max_distance && *hit_time < min_distance) {
                min_distance = *hit_time;
                Vector3 hit_point = origin + direction * min_distance;
                // Note: hit_normal calculation is simplified here. A more robust solution
                // would compute the normal based on the exact point of impact on the capsule.
                Vector3 hit_normal = (hit_point - entity->GetPosition()).Normalized();
                 if (entity->GetCollisionShapeType() == CollisionShapeType::CAPSULE) {
                    auto capsule_entity = std::dynamic_pointer_cast<GraphEdgeEntity>(entity);
                    if (capsule_entity) {
                        Capsule c = capsule_entity->GetCapsule();
                        Vector3 closest_point_on_axis = ClosestPointOnSegment(hit_point, c.a, c.b);
                        hit_normal = (hit_point - closest_point_on_axis).Normalized();
                    }
                }
                closest_hit = RaycastHit{entity, hit_point, hit_normal, min_distance};
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

        std::set<std::pair<int, int>> checked_pairs;

        for (const auto& pair : GetAllEntities()) {
            auto& e1 = pair.second;

            // Broad phase AABB calculation
            Vector3 start_pos = e1->GetPreviousPosition();
            Vector3 end_pos = e1->GetPosition();
            float radius = e1->GetSize();
            float min_b[3], max_b[3];

            if (e1->GetCollisionShapeType() == CollisionShapeType::SPHERE) {
                min_b[0] = std::min(start_pos.x, end_pos.x) - radius;
                min_b[1] = std::min(start_pos.y, end_pos.y) - radius;
                min_b[2] = std::min(start_pos.z, end_pos.z) - radius;
                max_b[0] = std::max(start_pos.x, end_pos.x) + radius;
                max_b[1] = std::max(start_pos.y, end_pos.y) + radius;
                max_b[2] = std::max(start_pos.z, end_pos.z) + radius;
            } else { // Capsule
                auto capsule_entity = std::static_pointer_cast<GraphEdgeEntity>(e1);
                Capsule c = capsule_entity->GetCapsule();
                min_b[0] = std::min(c.a.x, c.b.x) - radius;
                min_b[1] = std::min(c.a.y, c.b.y) - radius;
                min_b[2] = std::min(c.a.z, c.b.z) - radius;
                max_b[0] = std::max(c.a.x, c.b.x) + radius;
                max_b[1] = std::max(c.a.y, c.b.y) + radius;
                max_b[2] = std::max(c.a.z, c.b.z) + radius;
            }

            rtree_.Search(min_b, max_b, [&](int id2) {
                if (e1->GetId() == id2) return true;

                int id1 = e1->GetId();
                if (id1 > id2) std::swap(id1, id2);
                if (checked_pairs.count({id1, id2})) return true;
                checked_pairs.insert({id1, id2});

                auto e2 = GetEntity(id2);
                if (e2) {
                    // Narrow phase dispatch
                    std::optional<float> time_of_impact;
                    if (e1->GetCollisionShapeType() == CollisionShapeType::SPHERE && e2->GetCollisionShapeType() == CollisionShapeType::SPHERE) {
                        time_of_impact = SweptSphereSphere(*e1, *e2);
                    } else if (e1->GetCollisionShapeType() == CollisionShapeType::SPHERE && e2->GetCollisionShapeType() == CollisionShapeType::CAPSULE) {
                        auto capsule_entity = std::static_pointer_cast<GraphEdgeEntity>(e2);
                        time_of_impact = SphereCapsuleIntersect(*e1, *capsule_entity);
                    } else if (e1->GetCollisionShapeType() == CollisionShapeType::CAPSULE && e2->GetCollisionShapeType() == CollisionShapeType::SPHERE) {
                         auto capsule_entity = std::static_pointer_cast<GraphEdgeEntity>(e1);
                        time_of_impact = SphereCapsuleIntersect(*e2, *capsule_entity);
                    }
                    // Capsule-capsule not implemented

                    if (time_of_impact) {
                        e1->OnCollision(*e2);
                        e2->OnCollision(*e1);
                    }
                }
                return true;
            });
        }
    }
};

} // namespace Boidsish
