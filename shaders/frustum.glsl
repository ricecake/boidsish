#ifndef FRUSTUM_GLSL
#define FRUSTUM_GLSL

#include "types/frustum.glsl"

// Check if a point is inside a frustum defined by 6 planes
bool isPointInFrustumExtended(vec3 point, vec4 planes[6]) {
	for (int i = 0; i < 6; i++) {
		if (dot(planes[i].xyz, point) + planes[i].w < 0.0) {
			return false;
		}
	}
	return true;
}

// Check if a sphere is inside or intersecting a frustum
bool isSphereInFrustumExtended(vec3 center, float radius, vec4 planes[6]) {
	for (int i = 0; i < 6; i++) {
		if (dot(planes[i].xyz, center) + planes[i].w < -radius) {
			return false;
		}
	}
	return true;
}

// Check if an AABB is inside or intersecting a frustum
bool isAABBInFrustumExtended(vec3 minCorner, vec3 maxCorner, vec4 planes[6]) {
	for (int i = 0; i < 6; i++) {
		vec3 normal = planes[i].xyz;
		// Find the positive vertex (corner most in direction of plane normal)
		vec3 pVertex = vec3(
			normal.x >= 0.0 ? maxCorner.x : minCorner.x,
			normal.y >= 0.0 ? maxCorner.y : minCorner.y,
			normal.z >= 0.0 ? maxCorner.z : minCorner.z
		);
		if (dot(normal, pVertex) + planes[i].w < 0.0) {
			return false;
		}
	}
	return true;
}

// Compatibility wrappers for the default frustum
bool isPointInFrustum(vec3 point) {
	return isPointInFrustumExtended(point, frustumPlanes);
}

bool isSphereInFrustum(vec3 center, float radius) {
	return isSphereInFrustumExtended(center, radius, frustumPlanes);
}

bool isAABBInFrustum(vec3 minCorner, vec3 maxCorner) {
	return isAABBInFrustumExtended(minCorner, maxCorner, frustumPlanes);
}

#endif
