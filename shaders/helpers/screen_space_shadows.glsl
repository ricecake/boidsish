#ifndef SCREEN_SPACE_SHADOWS_GLSL
#define SCREEN_SPACE_SHADOWS_GLSL

#include "depth.glsl"
#include "../temporal_data.glsl"
#include "fast_noise.glsl"

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
	// Bias should be larger than typical depth buffer precision errors
	vec3 p_start = worldPos + normal * (0.1 * worldScale) + lightDir * (0.4 * worldScale);

	float t = 0.5 * worldScale;
	float maxDist = 80.0 * worldScale; // Local shadows only
	float closest = 1.0;
	int   iter = 0;

	// Raymarching
	// We use a smaller step size and more iterations for reliability on thin objects
	float stepSize = 0.5 * worldScale;

	// Temporal/Spatial dithering to hide banding and stamping
	// We use worldPos and time for a stable but varied dither pattern
	float dither = (fastWorley3d(worldPos * 5.0 + time) * 0.5 + 0.5) * stepSize;
	t += dither;

	while (t < maxDist && iter < 120) {
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

		// Sample depth from previous frame
		float sampledDepth = textureLod(u_hizTexture, prevUV, 0.0).r;
		float depthDiff = rayLinearDepth - sampledDepth;

		// Assumption of object thickness to avoid shadows being cast by the background
		// Trees can be quite thick (foliage/trunks).
		float thickness = (10.0 + t * 0.1) * worldScale;

		if (depthDiff > 0.0) {
			// Ray is behind sampled geometry
			if (depthDiff < thickness) {
				// We found an occluder.
				// Apply a small fade near the thickness edge to reduce artifacts
				float hitStrength = smoothstep(thickness, thickness * 0.7, depthDiff);
				return mix(1.0, 0.0, hitStrength);
			}
			// Passing behind object - continue search
		} else {
			// Ray is in front of geometry. Calculate contact hardening penumbra.
			// Formula: k * (distance_to_geometry / distance_from_source)
			closest = min(closest, 12.0 * ((-depthDiff) / t));
		}

		t += stepSize;
	}

	return clamp(closest, 0.0, 1.0);
}

#endif // SCREEN_SPACE_SHADOWS_GLSL
