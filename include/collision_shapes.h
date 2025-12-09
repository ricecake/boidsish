#pragma once

#include "boidsish.h"
#include <optional>
#include <algorithm>

namespace Boidsish {

// Represents a capsule shape for collision detection
struct Capsule {
    Vector3 a; // Start point of the capsule's inner segment
    Vector3 b; // End point of the capsule's inner segment
    float radius;
};

// Finds the closest point on a line segment to a given point
inline Vector3 ClosestPointOnSegment(const Vector3& p, const Vector3& a, const Vector3& b) {
    Vector3 ab = b - a;
    if (ab.MagnitudeSquared() < 1e-6f) return a;
    float t = (p - a).Dot(ab) / ab.MagnitudeSquared();
    return a + ab * std::max(0.0f, std::min(1.0f, t));
}

// Forward declarations for intersection functions
std::optional<float> RayCapsuleIntersect(const Vector3& origin, const Vector3& direction, const Capsule& capsule);
std::optional<float> SphereCapsuleIntersect(const Entity& sphere, const class GraphEdgeEntity& capsule_entity);

} // namespace Boidsish
