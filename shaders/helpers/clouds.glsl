#ifndef HELPERS_CLOUDS_GLSL
#define HELPERS_CLOUDS_GLSL

#include "fast_noise.glsl"

// Cloud density calculation helper
// Returns a density value [0, 1+] based on world-space position
float calculateCloudDensity(
	vec3  p,
	float weatherMap,
	float cloudAltitude,
	float cloudThickness,
	float cloudDensityBase,
	float cloudCoverage,
	float worldScale,
	float time,
	bool simplified
) {
	// Dynamic ceiling and floor based on weatherMap
	// Dense areas (high weatherMap) get lower floor and much higher ceiling (tall clouds)
	// Sparse areas get higher floor and thinner layer (wispy clouds)
	float floorOffset = mix(20.0, -10.0, weatherMap) * worldScale;
	float ceilingOffset = mix(10.0, 300.0, weatherMap * weatherMap) * worldScale;

	float baseFloor = cloudAltitude * worldScale + floorOffset;
	float baseCeiling = (cloudAltitude + cloudThickness) * worldScale + ceilingOffset;
	float currentThickness = baseCeiling - baseFloor;

	if (p.y < baseFloor || p.y > baseCeiling) return 0.0;

	// Height-based tapering with a more natural profile
	float h = (p.y - baseFloor) / max(currentThickness, 0.001);
	float tapering = smoothstep(0.0, 0.15, h) * smoothstep(1.0, 0.7, h);

	// Tall cloud profile: more density at the bottom, anvil-like top
	float densityProfile = mix(tapering, pow(tapering, 0.5), weatherMap);

	float coverageThreshold = 1.0 - cloudCoverage;
	float localDensity = weatherMap * cloudDensityBase;

	if (simplified) {
		return smoothstep(coverageThreshold, coverageThreshold + 0.2, localDensity) * densityProfile * cloudDensityBase;
	}

	// Base noise for cloud shapes
	vec3 p_warped = p + 5.0 * fastCurl3d(p / (400.0 * worldScale) + time * 0.02);
	vec3 p_scaled = p_warped / (700.0 * worldScale);

	float baseNoise = 1.0-fastWorley3d(p_scaled + time * 0.005);

	// Add ridges and textures for definition
	float ridges = fastRidge3d(p_warped / (600.0 * worldScale));
	float detail = fastFbm3d(p_warped / (450.0 * worldScale) + time * 0.01) * 0.5 + 0.5;

	// Combine noises
	float finalNoise = baseNoise * (0.6 + 0.4 * ridges);
	finalNoise = mix(finalNoise, finalNoise * detail, 0.3);

	// Apply coverage and local density
	float density = smoothstep(coverageThreshold, coverageThreshold + 0.4, finalNoise * weatherMap);

	// Giant tall clouds vs wispy things
	// High weatherMap = tall, dense, sharp
	// Low weatherMap = wispy, thin, soft
	float wispyFactor = smoothstep(0.2, 035, weatherMap);
	density *= mix(0.6, 1.0, wispyFactor);

	return density * densityProfile * cloudDensityBase * 3.0;
}

#endif // HELPERS_CLOUDS_GLSL
