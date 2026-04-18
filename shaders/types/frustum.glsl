#ifndef FRUSTUM_TYPES_GLSL
#define FRUSTUM_TYPES_GLSL

// Frustum UBO for GPU-side frustum culling (binding point 3)
// Each plane is stored as vec4: xyz = normal, w = distance
layout(std140, binding = [[FRUSTUM_BINDING]]) uniform FrustumData {
	vec4  frustumPlanes[6]; // Left, Right, Bottom, Top, Near, Far
	vec3  frustumCameraPos; // Camera position for distance-based LOD
	float frustumPadding;   // Padding for std140 alignment
};

#endif
