#include "../include/collision_shapes.h"
#include "../include/graph_entities.h"
#include <algorithm>

namespace Boidsish {

// Ray-capsule intersection
// Based on the algorithm by Christer Ericson in "Real-Time Collision Detection"
std::optional<float> RayCapsuleIntersect(const Vector3& origin, const Vector3& direction, const Capsule& capsule) {
    Vector3 cap_axis = capsule.b - capsule.a;
    Vector3 oc = origin - capsule.a;

    float card = cap_axis.Dot(direction);
    float caoc = cap_axis.Dot(oc);
    float caca = cap_axis.Dot(cap_axis);

    float a = caca - card * card;
    float b = caca * oc.Dot(direction) - caoc * card;
    float c = caca * oc.Dot(oc) - caoc * caoc - capsule.radius * capsule.radius * caca;

    float h = b * b - a * c;
    if (h < 0.0f) {
        return std::nullopt; // No intersection
    }

    float t = (-b - std::sqrt(h)) / a;

    // Check if the intersection point is within the capsule's cylinder part
    float y = caoc + t * card;
    if (y > 0.0f && y < caca) {
        return t;
    }

    // Check intersection with the end caps (spheres)
    Vector3 p = (y <= 0.0f) ? capsule.a : capsule.b;
    Vector3 op = origin - p;
    float card2 = direction.Dot(op);
    float c2 = op.Dot(op) - capsule.radius * capsule.radius;

    float h2 = card2 * card2 - c2;
    if (h2 > 0.0f) {
        return -card2 - std::sqrt(h2);
    }

    return std::nullopt;
}

// Swept-sphere vs capsule intersection test
std::optional<float> SphereCapsuleIntersect(const Entity& sphere_entity, const GraphEdgeEntity& capsule_entity) {
    // For simplicity in this step, we will treat the capsule as static and the sphere as moving.
    // This is a reasonable approximation for graph edges.
    Capsule capsule = capsule_entity.GetCapsule();
    Vector3 sphere_start = sphere_entity.GetPreviousPosition();
    Vector3 sphere_vel = sphere_entity.GetPosition() - sphere_start;
    float sphere_radius = sphere_entity.GetSize();

    // The problem can be reduced to a ray-capsule intersection, where the ray is the
    // sphere's velocity and the capsule is "inflated" by the sphere's radius.
    Capsule inflated_capsule = {capsule.a, capsule.b, capsule.radius + sphere_radius};

    auto hit_time = RayCapsuleIntersect(sphere_start, sphere_vel.Normalized(), inflated_capsule);

    if (hit_time && *hit_time <= sphere_vel.Magnitude()) {
        return *hit_time / sphere_vel.Magnitude();
    }

    return std::nullopt;
}


} // namespace Boidsish
