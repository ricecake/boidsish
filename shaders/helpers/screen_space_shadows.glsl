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
	vec3 p_start = worldPos + normal * (0.1 * worldScale) + lightDir * (0.2 * worldScale);

	float t = 0.2 * worldScale;
	float maxDist = 60.0 * worldScale; // Local shadows only
	float closest = 1.0;
	int   iter = 0;

	// Hierarchical Hi-Z raymarching
	while (t < maxDist && iter < 40) {
		iter++;
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

		float rayLinearDepth = linearizeDepth(prevNdc.z * 0.5 + 0.5, td.nearPlane, td.farPlane);

		// Determine mip level based on step size for hierarchical skipping
		// Larger t -> larger screenspace footprint -> higher mip
		int mip = clamp(int(log2(t / worldScale)), 0, 4);

		// Sample Hi-Z (stores MAX linearized depth in each tile)
		float hizMaxDepth = textureLod(u_hizTexture, prevUV, float(mip)).r;

		// If ray is still "in front" of the max depth in this tile, we can skip ahead
		if (rayLinearDepth < hizMaxDepth - (0.5 * worldScale)) {
			// Skip ahead proportional to mip level
			t += (1.0 + float(mip)) * worldScale;
			continue;
		}

		// At LOD 0, perform actual occlusion and penumbra tests
		float sampledDepth = textureLod(u_hizTexture, prevUV, 0.0).r;
		float depthDiff = rayLinearDepth - sampledDepth;

		float thickness = 4.0 * worldScale;

		if (depthDiff > 0.0) {
			// Ray is behind sampled geometry
			if (depthDiff < thickness) {
				return 0.0; // Hit!
			}
			// Passing behind object - no hit, but don't skip
		} else {
			// Ray is clearing the geometry. Calculate closeness for soft penumbra.
			// Formula: penumbra = shadow_factor * (dist_to_geometry / distance_traveled)
			// Matches terrain shadow logic: closest = min(closest, k * (depth_diff / t))
			closest = min(closest, 8.0 * ((-depthDiff) / t));
		}

		t += 1.0 * worldScale;
	}

	return clamp(closest, 0.0, 1.0);
}

#endif // SCREEN_SPACE_SHADOWS_GLSL
