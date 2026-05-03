#ifndef HELPERS_CLOUDS_GLSL
#define HELPERS_CLOUDS_GLSL

#include "../lighting.glsl"
#include "fast_noise.glsl"
#include "math.glsl"
#include "wind.glsl"

float cloudPhase(float cosTheta) {
	// Dual-lobe Henyey-Greenstein for forward and back scattering
	// Blended with a large isotropic component to ensure visibility at all angles
	float hg = mix(henyeyGreenstein(cloudPhaseG1, cosTheta), henyeyGreenstein(cloudPhaseG2, cosTheta), cloudPhaseAlpha);
	return mix(hg, 1.0 / (4.0 * PI), cloudPhaseIsotropic);
}

float beerPowder(float d, float local_d) {
	// Approximation of multiple scattering (Beer-Powder law)
	// Ensuring sunny side isn't black when d is small
	return max(
		exp(-d),
		exp(-d * cloudPowderScale) * cloudPowderMultiplier * (1.0 - exp(-local_d * cloudPowderLocalScale))
	);
}

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

// Warp cloud position away from the camera's view axis (capsule-based sliding warp)
// Returns the warped position and a fade factor for density
vec3 getWarpedCloudPos(vec3 p, out float fade) {
	fade = 1.0;
	if (cloudWarp <= 0.0)
		return p;

	// vec3  relP = p - viewPos;
	// float projection = dot(relP, viewDir);

	// Capsule distance: distance to the forward ray starting at viewPos
	// vec3  axisPoint = viewPos + viewDir * max(0.0, projection);
	// vec3  toP = p - axisPoint;
	// float d = length(toP);
	float R = cloudWarp * worldScale;

	// New uniform or constant for how far the bubble extends
	float capsuleLength = cloudWarp * worldScale * 5.0; // Example ratio
	vec3  ap = p - viewPos;
	// t is the projection of the current point onto the view direction
	float t = dot(ap, viewDir);
	// Clamp the projection to the segment bounds [0, capsuleLength]
	float t_clamped = clamp(t, 0.0, capsuleLength);
	// Find the closest point on the clamped segment
	vec3 axisPoint = viewPos + viewDir * t_clamped;
	// Vector from the closest point to the actual point
	vec3 toP = p - axisPoint;
	// d is now the distance to a capsule core, rather than a cylinder core
	float d = length(toP);

	// To "push" clouds out, we sample from a position CLOSER to the axis.
	// This maps the region [R, inf] to [0, inf].
	// float d_sampling = max(0.0, d - R);
	float d_sampling = d * ((d * d) / (d * d + R * R));
	// float d_sampling = d * (1.0 - exp(-d / R));
	// float d_sampling = d * (d / (d + R));
	float scale = d_sampling / max(d, 0.0001);

	// Fade out density in the inner core to create a clean hole and avoid sampling artifacts
	fade = smoothstep(R * 0.1, R, d);
	// fade = 1;

	return axisPoint + toP * scale;
}

CloudLayer computeCloudLayer(CloudWeather weather, CloudProperties props, vec3 p) {
	vec4 scalars = getWeatherScalarsAtPosition(p);
	float humidity = scalars.y;

	// Use heightMap and local humidity for vertical expansion
	float variation = weather.heightMap * mix(0.5, 1.5, humidity);

	float floorOffset = mix(50.0, -150.0, variation);
	float ceilingOffset = mix(20.0, 1000.0, variation);

	float altitudeOffset = mix(-100.0, 800.0, variation);

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

	// 1. Initial Gating - avoid marching if initial density doesn't warrant it
	float coverageThreshold = 1.0 - props.coverage;
	float gatingThreshold = coverageThreshold + 0.1;
	if (weather.weatherMap < gatingThreshold * 0.5) return 0.0;

	// 2. Local Weather Scaling
	vec4 scalars = getWeatherScalarsAtPosition(p);
	float humidity = scalars.y;
	float tempFactor = clamp((scalars.x - 273.15) / 30.0, 0.0, 1.0); // 0C to 30C remapped

	// Boost coverage in humid/warm areas
	float weatherCoverage = props.coverage * mix(0.8, 1.2, humidity * (1.0 + tempFactor * 0.2));
	coverageThreshold = 1.0 - weatherCoverage;

	// 3. Wind Advection - Stronger at heights
	vec3 wind = getWindAtPosition(p);
	vec3 advectionOffset = wind * 5.0; // Scale for visual impact
	vec3 p_advected = p - advectionOffset * (0.5 + h * 2.0);

	// 4. Anvil and Vertical Shape Profile
	// h is [0, 1] relative to layer.
	// Anvil factor: more prominent for tall clouds and in high-weatherMap areas
	float anvilFactor = smoothstep(0.2, 0.8, h) * weather.heightMap;
	float tapering = smoothstep(0.0, 0.15, h) * smoothstep(1.0, 0.7, h);

	// Mix between bottom-heavy and anvil profile
	float bottomHeavy = tapering;
	float anvil = pow(tapering, mix(0.7, 0.2, weather.heightMap)) * mix(1.0, 1.5, h);
	float densityProfile = mix(bottomHeavy, anvil, anvilFactor);

	if (simplified) {
		// Include base Worley noise so shadow patterns match the full cloud shapes
		float baseNoise = fastWorley3d(p_advected / (50000.0 * props.worldScale) + time * 0.0005);
		float baseDensity = baseNoise * weather.weatherMap;
		return smoothstep(coverageThreshold, coverageThreshold + 0.4, baseDensity) * densityProfile * props.densityBase;
	}

	// 5. Multi-Scale Cloud Noise
	vec3 p_warped = p_advected + 5.0 * fastCurl3d(p_advected / (900.0 * props.worldScale) + time * 0.002);

	// Base Worley noise with height-dependent remapping to vary cone shapes
	float baseNoise = fastWorley3d(p_warped / (50000.0 * props.worldScale) + time * 0.0005);

	// Implement "Roll": Billowy edges that vary with height and weather
	// Vary the remapping thresholds based on altitude to prevent uniform cone shapes
	float lowerThreshold = mix(0.4, 0.2, anvilFactor);
	float upperThreshold = mix(0.1, 0.4, anvilFactor * (1.0 - h));
	float rollFactor = remap(h, 0.0, 1.0, lowerThreshold, upperThreshold);
	float rolledNoise = remap(baseNoise, rollFactor, 1.0, 0.0, 1.0);

	// Add ridges and textures for definition
	float ridges = fastRidge3d(p_warped / (1600.0 * props.worldScale));
	float detail = fastFbm3d(p_warped / (1450.0 * props.worldScale) + time * 0.001) * 0.5 + 0.5;

	// Combine noises
	float finalNoise = rolledNoise * (0.6 + 0.4 * ridges);
	finalNoise = mix(finalNoise, finalNoise * detail, 0.3);

	// Apply coverage and local density
	float baseDensity = finalNoise * weather.weatherMap;
	float density = smoothstep(coverageThreshold, coverageThreshold + 0.4, baseDensity);

	// Gated early-out for low density
	if (density <= 0.0) return 0.0;

	// Add "Edge Wisps": high-frequency FBM at the boundaries
	if (density < 0.3) {
		float wisps = fastFbm3d(p_warped / (400.0 * props.worldScale) + time * 0.05) * 0.5 + 0.5;
		float wispMask = smoothstep(0.3, 0.0, density);
		density += wisps * wispMask * 0.15 * weather.weatherMap;
	}

	// Giant tall clouds vs wispy things
	float wispyFactor = smoothstep(0.2, 0.35, weather.weatherMap);
	density *= mix(0.6, 1.0, wispyFactor);

	return density * densityProfile * props.densityBase * 3.0;
}

float calculateCloudShadowDensity(vec3 p, CloudWeather weather, CloudLayer layer, CloudProperties props, float time) {
	return 10.0 * calculateCloudDensity(p, weather, layer, props, time, true);
}

/**
 * High-level function to evaluate cloud shadow density at a specific world XZ position.
 * This encapsulates the logic used by both the shadow map generator and the runtime fallback.
 */
float evaluateCloudShadowDensityAtWorldPos(vec2 worldXZ, float time) {
	// Replicate logic from calculateCloudShadow in lighting.glsl
	// This ensures the shadow map matches what the raymarch would have produced
	float shadowAltitude = cloudAltitude + cloudThickness * 0.5;
	float scaledCloudAltitude = shadowAltitude * worldScale;
	vec3  cloudPos = vec3(worldXZ.x, scaledCloudAltitude, worldXZ.y);

	float weatherMap = (fastWorley3d(vec3(cloudPos.xz / (4000.0 * worldScale), time * 0.001)) * 0.5 + 0.5);
	float heightMap = (fastWorley3d(vec3(cloudPos.xz / (2500.0 * worldScale), time * 0.0004)) * 0.5 + 0.5);

	CloudWeather weather;
	weather.weatherMap = weatherMap;
	weather.heightMap = heightMap;

	CloudProperties props;
	props.altitude = cloudAltitude;
	props.thickness = cloudThickness;
	props.densityBase = cloudDensity;
	props.coverage = cloudCoverage;
	props.worldScale = worldScale;

	CloudLayer layer = computeCloudLayer(weather, props, cloudPos);

	// Sample at the center of the dynamic layer
	cloudPos.y = (layer.baseFloor + layer.baseCeiling) * 0.5;

	return calculateCloudShadowDensity(cloudPos, weather, layer, props, time);
}

#endif // HELPERS_CLOUDS_GLSL
