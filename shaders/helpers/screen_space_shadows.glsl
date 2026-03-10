#ifndef SCREEN_SPACE_SHADOWS_GLSL
#define SCREEN_SPACE_SHADOWS_GLSL

#include "depth.glsl"
#include "../temporal_data.glsl"

uniform sampler2D u_hizTexture;

/**
 * Calculate screen-space shadow coverage using Hi-Z raymarching.
 * This complements terrain shadows by providing shadows for decor objects (trees, etc.)
 * based on the depth buffer information from the previous frame.
 *
 * @param worldPos Fragment world position
 * @param normal Fragment normal
 * @param lightDir Direction from fragment TO light
 * @return 0.0 if in shadow, 1.0 if lit, or intermediate for soft shadows
 */
float screenSpaceShadowCoverage(vec3 worldPos, vec3 normal, vec3 lightDir) {
	// Optimization: Skip if light is below horizon
	if (lightDir.y <= 0.0)
		return 0.0;

	// Start with a small bias to prevent self-shadowing
	vec3 p_start = worldPos + lightDir * (1.51 * worldScale);

	float t = 0.1 * worldScale;
	float maxDist = 60.0 * worldScale; // Local shadows only
	float closest = 1.0;
	int   maxSteps = 40;
	float stepSize = 1.5 * worldScale;

	for (int i = 0; i < maxSteps && t < maxDist; i++) {
		vec3 p = p_start + lightDir * t;

		// Project to previous frame's screen space using temporal data
		vec4 prevClip = td.prevViewProjection * vec4(p, 1.0);
		if (prevClip.w <= 0.0)
			break;

		vec3 prevNdc = prevClip.xyz / prevClip.w;
		vec2 prevUV = prevNdc.xy * 0.5 + 0.5;

		// Stop if ray goes off screen as we have no depth info there
		if (prevUV.x < 0.0 || prevUV.x > 1.0 || prevUV.y < 0.0 || prevUV.y > 1.0)
			break;

		// Sample Hi-Z (which stores linearized max depth from previous frame)
		// Mip 0 is full resolution linearized depth
		float sampledLinearDepth = textureLod(u_hizTexture, prevUV, 0.0).r;
		float rayLinearDepth = linearizeDepth(prevNdc.z * 0.5 + 0.5, td.nearPlane, td.farPlane);

		float depthDiff = sampledLinearDepth - rayLinearDepth; // Positive if ray is in front of geometry

		// Thickness heuristic: Screen-space depth doesn't tell us the "back" of objects.
		// We assume objects have a certain world-space thickness.
		float thickness = 5.0;

		if (depthDiff < 0.0) {
			// Ray is behind the sampled depth
			if (-depthDiff < thickness) {
				return 0.0; // Hit! Occluded by screen-space geometry
			}
			// If it's deeper than thickness, we assume it's passing behind the object (e.g. through a tree)
		}
		// Ray is in front of geometry. Calculate "closeness" for soft shadows.
		// Smaller depthDiff / t means we are passing very close to an edge.
		closest = min(closest, 8.0 * (abs(depthDiff) / t));

		t += stepSize;
	}

	return closest;
}

#endif // SCREEN_SPACE_SHADOWS_GLSL
