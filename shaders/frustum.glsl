#ifndef FRUSTUM_GLSL
#define FRUSTUM_GLSL

// Frustum UBO for GPU-side frustum culling (binding point 3)
// Each plane is stored as vec4: xyz = normal, w = distance
layout(std140, binding = 3) uniform FrustumData {
	vec4  frustumPlanes[6]; // Left, Right, Bottom, Top, Near, Far
	vec3  frustumCameraPos; // Camera position for distance-based LOD
	float frustumPadding;   // Padding for std140 alignment
};

// Check if a point is inside the frustum
// Returns true if inside, false if outside
bool isPointInFrustum(vec3 point) {
	for (int i = 0; i < 6; i++) {
		if (dot(frustumPlanes[i].xyz, point) + frustumPlanes[i].w < 0.0) {
			return false;
		}
	}
	return true;
}

// Check if a sphere is inside or intersecting the frustum
// Returns true if inside or intersecting, false if completely outside
bool isSphereInFrustum(vec3 center, float radius) {
	for (int i = 0; i < 6; i++) {
		if (dot(frustumPlanes[i].xyz, center) + frustumPlanes[i].w < -radius) {
			return false;
		}
	}
	return true;
}

// Check if an AABB is inside or intersecting the frustum
// Returns true if inside or intersecting, false if completely outside
bool isAABBInFrustum(vec3 minCorner, vec3 maxCorner) {
	for (int i = 0; i < 6; i++) {
		vec3 normal = frustumPlanes[i].xyz;
		// Find the positive vertex (corner most in direction of plane normal)
		vec3 pVertex = vec3(
			normal.x >= 0.0 ? maxCorner.x : minCorner.x,
			normal.y >= 0.0 ? maxCorner.y : minCorner.y,
			normal.z >= 0.0 ? maxCorner.z : minCorner.z
		);
		if (dot(normal, pVertex) + frustumPlanes[i].w < 0.0) {
			return false;
		}
	}
	return true;
}

#endif
