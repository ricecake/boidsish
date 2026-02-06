#ifndef HELPERS_FOG_GLSL
#define HELPERS_FOG_GLSL

#include "lighting.glsl"

/**
 * Calculate height-based exponential fog.
 * @param start Ray start position (camera)
 * @param end Ray end position (world fragment)
 * @param density Fog density at height 0
 * @param heightFalloff Reciprocal of fog height (1.0 / hazeHeight)
 */
float getHeightFog(vec3 start, vec3 end, float density, float heightFalloff) {
	float dist = length(end - start);
	vec3  dir = (end - start) / dist;

	float fog;
	if (abs(dir.y) < 0.0001) {
		fog = density * exp(-heightFalloff * start.y) * dist;
	} else {
		fog = (density / (heightFalloff * dir.y)) * (exp(-heightFalloff * start.y) - exp(-heightFalloff * end.y));
	}
	return 1.0 - exp(-max(0.0, fog));
}

/**
 * Apply fog to a given color using global Lighting parameters.
 */
vec3 apply_fog(vec3 color, vec3 worldPos) {
    float fogFactor = getHeightFog(viewPos, worldPos, hazeDensity, 1.0 / (hazeHeight * worldScale + 0.001));
    return mix(color, hazeColor, fogFactor);
}

#endif // HELPERS_FOG_GLSL
