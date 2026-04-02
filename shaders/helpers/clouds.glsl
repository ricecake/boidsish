#ifndef HELPERS_CLOUDS_GLSL
#define HELPERS_CLOUDS_GLSL

#include "fast_noise.glsl"
#include "math.glsl"

struct CloudProperties {
	float altitude;
	float thickness;
	float densityBase;
	float coverage;
	float worldScale;
};

struct CloudWeather {
	float weatherMap;
	float heightMap;
};

struct CloudLayer {
	float baseFloor;
	float baseCeiling;
	float thickness;
};

CloudLayer computeCloudLayer(CloudWeather weather, CloudProperties props) {
	// Use heightMap for vertical expansion to decouple it from horizontal coverage
	float floorOffset = mix(20.0, -50.0, weather.heightMap);
	float ceilingOffset = mix(10.0, 500.0, weather.heightMap);

	float altitudeOffset = mix(0.0, 500.0, weather.heightMap);

	CloudLayer layer;
	layer.baseFloor = (altitudeOffset + props.altitude + floorOffset) * props.worldScale;
	layer.baseCeiling = (altitudeOffset + props.altitude + props.thickness + ceilingOffset) * props.worldScale;
	layer.thickness = max(layer.baseCeiling - layer.baseFloor, 0.001);
	return layer;
}

// Cloud density calculation helper
// Returns a density value [0, 1+] based on world-space position
float calculateCloudDensity(
	vec3            p,
	CloudWeather    weather,
	CloudLayer      layer,
	CloudProperties props,
	float           time,
	bool            simplified
) {
	if (p.y < layer.baseFloor || p.y > layer.baseCeiling)
		return 0.0;

	// Height-based tapering with a more natural profile
	float h = (p.y - layer.baseFloor) / layer.thickness;
	float tapering = smoothstep(0.0, 0.15, h) * smoothstep(1.0, 0.7, h);

	// Tall cloud profile: anvil-like top for tall clouds
	// Mix between a bottom-heavy profile and an anvil profile based on heightMap
	float bottomHeavy = tapering;
	float anvil = pow(tapering, mix(0.7, 0.3, weather.heightMap));
	float densityProfile = mix(bottomHeavy, anvil, h * weather.heightMap);

	float coverageThreshold = 1.0 - props.coverage;
	float localDensity = weather.weatherMap * props.densityBase;

	if (simplified) {
		return smoothstep(coverageThreshold, coverageThreshold + 0.2, localDensity) * densityProfile * props.densityBase;
	}

	// Base noise for cloud shapes
	vec3 p_warped = p + 5.0 * fastCurl3d(p / (900.0 * props.worldScale) + time * 0.02);
	vec3 p_scaled = p_warped / (50000.0 * props.worldScale);

	float baseNoise = fastWorley3d(p_scaled + time * 0.005);

	// Implement "Roll": Billowy edges that vary with height
	// We remap the base noise threshold based on the vertical position
	float rollFactor = remap(h, 0.0, 1.0, 0.4, 0.1);
	float rolledNoise = remap(baseNoise, rollFactor, 1.0, 0.0, 1.0);

	// Add ridges and textures for definition
	float ridges = fastRidge3d(p_warped / (1600.0 * props.worldScale));
	float detail = fastFbm3d(p_warped / (1450.0 * props.worldScale) + time * 0.01) * 0.5 + 0.5;

	// Combine noises
	float finalNoise = rolledNoise * (0.6 + 0.4 * ridges);
	finalNoise = mix(finalNoise, finalNoise * detail, 0.3);

	// Apply coverage and local density
	float baseDensity = finalNoise * weather.weatherMap;
	float density = smoothstep(coverageThreshold, coverageThreshold + 0.4, baseDensity);

	// Add "Edge Wisps": high-frequency FBM at the boundaries
	if (density > 0.0 && density < 0.3) {
		float wisps = fastFbm3d(p_warped / (400.0 * props.worldScale) + time * 0.05) * 0.5 + 0.5;
		float wispMask = smoothstep(0.3, 0.0, density);
		density += wisps * wispMask * 0.15 * weather.weatherMap;
	}

	// Giant tall clouds vs wispy things
	// High weatherMap = tall, dense, sharp
	// Low weatherMap = wispy, thin, soft
	float wispyFactor = smoothstep(0.2, 0.35, weather.weatherMap);
	density *= mix(0.6, 1.0, wispyFactor);

	return density * densityProfile * props.densityBase * 3.0;
}

#endif // HELPERS_CLOUDS_GLSL
