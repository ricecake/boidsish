#ifndef SHOCKWAVE_HELPER_GLSL
#define SHOCKWAVE_HELPER_GLSL

// Maximum number of simultaneous shockwaves (must match C++ kMaxShockwaves)
#define MAX_SHOCKWAVES [[MAX_SHOCKWAVES]]

// Shockwave data structure (matches ShockwaveGPUData in C++)
struct ShockwaveData {
	vec4 center_radius; // xyz = center, w = current_radius
	vec4 normal;        // xyz = normal, w = unused
	vec4 params;        // x = intensity, y = ring_width, z = max_radius, w = normalized_age
	vec4 color_unused;  // xyz = color, w = unused
};

// Shockwave UBO (binding point 4)
layout(std140, binding = 4) uniform Shockwaves {
	int           shockwave_count; // Padded to 16 bytes
	int           _pad1, _pad2, _pad3;
	ShockwaveData shockwaves[MAX_SHOCKWAVES];
};

/**
 * Calculate shockwave displacement at a given world position.
 * Returns a displacement vector that can be added to the world position.
 *
 * @param worldPos The world-space position of the vertex
 * @param localHeight The local height of the vertex (for swaying)
 * @param useSway If true, applies a swaying effect. If false, applies a general push.
 */
vec3 getShockwaveDisplacement(vec3 worldPos, float localHeight, bool useSway) {
	vec3 totalDisplacement = vec3(0.0);

	for (int i = 0; i < shockwave_count && i < MAX_SHOCKWAVES; ++i) {
		vec3  center = shockwaves[i].center_radius.xyz;
		float currentRadius = shockwaves[i].center_radius.w;
		float intensity = shockwaves[i].params.x;
		float ringWidth = shockwaves[i].params.y;
		float maxRadius = shockwaves[i].params.z;

		float dist = length(worldPos - center);
		if (dist > maxRadius || dist < 0.01)
			continue;

		float distFromRing = abs(dist - currentRadius);

		// Gaussian falloff for ring width
		float ringFactor = exp(-distFromRing * distFromRing / (2.0 * ringWidth * ringWidth));

		if (ringFactor > 0.001) {
			vec3 dir = normalize(worldPos - center);

			// Pressure wave profile: push out, then slight pull back
			float profile;
			if (dist < currentRadius) {
				profile = -0.7 * (1.0 - smoothstep(currentRadius - ringWidth, currentRadius, dist));
			} else {
				profile = 1.0 * (1.0 - smoothstep(currentRadius, currentRadius + ringWidth, dist));
			}

			float strength = ringFactor * intensity * profile * 1.5; // * 0.8;

			if (useSway) {
				// Sway effect: displacement scales with height
				totalDisplacement += dir * strength * max(0.0, localHeight);
			} else {
				// General push
				totalDisplacement += dir * strength;
			}
		}
	}

	return totalDisplacement;
}

#endif
