#ifndef HELPERS_FOG_GLSL
#define HELPERS_FOG_GLSL

#include "lighting.glsl"

layout(std140, binding = 6) uniform Atmosphere {
	vec4  hazeParams;   // x: density, y: height, zw: unused
	vec4  hazeColor;    // rgb: color, w: unused
	vec4  cloudParams;  // x: density, y: altitude, z: thickness, w: unused
	vec4  cloudColor;   // rgb: color, w: unused
};

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
 * Apply fog to a given color.
 */
vec3 applyFog(vec3 color, vec3 fragPos, vec3 camPos) {
    float fogFactor = getHeightFog(camPos, fragPos, hazeParams.x, 1.0 / (hazeParams.y * worldScale + 0.001));
    return mix(color, hazeColor.rgb, fogFactor);
}

#endif // HELPERS_FOG_GLSL
