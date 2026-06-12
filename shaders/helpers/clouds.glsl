#ifndef HELPERS_CLOUDS_GLSL
#define HELPERS_CLOUDS_GLSL

#include "../lighting.glsl"
#include "fast_noise.glsl"
#include "math.glsl"
#include "lygia/generative/random.glsl"


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
	float weatherMap; // Density/Coverage
	float heightMap;  // Vertical expansion and variety
	float cellID;     // Per-cell variety
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
	return p;
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

CloudWeather computeCloudWeather(vec3 p, CloudProperties props) {
	vec2  weatherData = fastWorley3dID(vec3(p.x, 0.0, p.z) / (10000.0 * worldScale));
	float weatherMap = 1.0 - weatherData.x; // Worley distance for coverage
	float cellID = weatherData.y;           // Cell ID for variety

	float heightMap = fastWorley3d(vec3(p.x, (3.0 * time), p.z) / (7500.0 * worldScale)) * 0.5 + 0.5;

	CloudWeather weather;
	weather.weatherMap = weatherMap;
	weather.heightMap = heightMap;
	weather.cellID = cellID;

	return weather;
}

CloudLayer computeCloudLayer(CloudWeather weather, CloudProperties props) {
	// Use heightMap for dramatic vertical expansion for specific weather cells
	// Tall clouds (cumulonimbus) can be 5-10x thicker than base thickness
	float verticalExpansion = mix(1.0, 8.0, weather.heightMap * weather.weatherMap);

	CloudLayer layer;
	layer.baseFloor = props.altitude * props.worldScale;
	layer.baseCeiling = (props.altitude + props.thickness * verticalExpansion) * props.worldScale;
	layer.thickness = max(layer.baseCeiling - layer.baseFloor, 0.001);
	return layer;
}

vec3 getCloudAdvectionOffset(float h, float worldScale, float time) {
	float angle = cloudFlowDirection;
	vec2  flowDir = vec2(cos(angle), sin(angle));

	// Dramatic non-linear shear profile
	float shear = h * h * cloudFlowHeightScale * 2.0;

	vec3 advect = vec3(flowDir.x, 0.0, flowDir.y) * time * cloudFlowSpeed * worldScale * 10.0;
	advect.xz += flowDir * shear * worldScale * 1000.0;

	return advect;
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

	// Height within the dynamic layer [0, 1]
	float h = (p.y - layer.baseFloor) / layer.thickness;

	// Vertical sampling expansion: tall clouds are stretched vertically to maintain detail
	float verticalStretch = 1.0 + weather.heightMap * 3.0;
	float py_scaled = (p.y - layer.baseFloor) / verticalStretch + layer.baseFloor;

	// Apply advection
	vec3 advect = getCloudAdvectionOffset(h, props.worldScale, time);
	vec3 p_advected = vec3(p.x, py_scaled, p.z) + advect;

	// Domain warping using curl noise
	vec3 curl = fastCurl3d(p_advected / (2000.0 * props.worldScale * cloudCurlFrequency));
	vec3 p_warped = p_advected + curl * cloudCurlStrength * weather.heightMap * props.worldScale * 500.0;

	// Base shape noise (Worley)
	vec3 p_base = p_warped / (5000.0 * props.worldScale);
	float baseShape = fastWorley3d(p_base);

	// Use coverage and weatherMap to determine the base density
	float coverage = props.coverage * weather.weatherMap;
	float baseDensity = remap(baseShape, 1.0 - coverage, 1.0, 0.0, 1.0);

	if (baseDensity <= 0.0) return 0.0;

	// Height-based tapering
	// Tall clouds (high heightMap) have an anvil-like profile
	float anvil = pow(h, mix(0.5, 0.2, weather.heightMap)) * (1.0 - smoothstep(0.7, 1.0, h));
	float standard = smoothstep(0.0, 0.2, h) * (1.0 - smoothstep(0.8, 1.0, h));
	float tapering = mix(standard, anvil, weather.heightMap);

	baseDensity *= tapering;

	if (simplified || baseDensity <= 0.0) {
		return baseDensity * props.densityBase * 2.0;
	}

	// Erosion / Detail
	vec3 p_detail = p_warped / (1200.0 * props.worldScale);
	float fbm = fastFbm3d(p_detail) * 0.5 + 0.5;
	float ridge = fastRidge3d(p_detail * 2.0);

	// Differentiate erosion based on cellID
	float erosionFactor = mix(fbm, ridge, fract(weather.cellID * 13.37));
	float erosion = remap(erosionFactor, 0.0, 1.0, 0.2, 0.8);

	// Apply erosion: subtract detail from the edges
	float density = remap(baseDensity, erosion * (1.0 - h), 1.0, 0.0, 1.0);

	// Edge wisps
	float wisps = fastFbm3d(p_warped / (500.0 * props.worldScale + time * 10.0)) * 0.5 + 0.5;
	density += wisps * (1.0 - baseDensity) * 0.2 * weather.weatherMap;

	return clamp(density, 0.0, 1.0) * props.densityBase * 3.0;
}

float calculateCloudShadowDensity(vec3 p, CloudWeather weather, CloudLayer layer, CloudProperties props, float time) {
	return 10.0 * calculateCloudDensity(p, weather, layer, props, time, true);
}

/**
 * High-level function to evaluate cloud shadow density at a specific world XZ position.
 * This encapsulates the logic used by both the shadow map generator and the runtime fallback.
 */
float evaluateCloudShadowDensityAtWorldPos(vec2 worldXZ, float time) {
	CloudProperties props;
	props.altitude = cloudAltitude;
	props.thickness = cloudThickness;
	props.densityBase = cloudDensity;
	props.coverage = cloudCoverage;
	props.worldScale = worldScale;

	// Use a dummy cloud position to evaluate weather/layer at this XZ
	vec3  basePos = vec3(worldXZ.x, props.altitude * props.worldScale, worldXZ.y);
	CloudWeather weather = computeCloudWeather(basePos, props);
	CloudLayer layer = computeCloudLayer(weather, props);

	// Integrate density vertically through the expanded layer to capture the full shadow
	float totalDensity = 0.0;
	const int shadowSteps = 4;
	float stepSize = layer.thickness / float(shadowSteps);

	for (int i = 0; i < shadowSteps; i++) {
		vec3 p = vec3(worldXZ.x, layer.baseFloor + (float(i) + 0.5) * stepSize, worldXZ.y);
		totalDensity += calculateCloudDensity(p, weather, layer, props, time, true);
	}

	return totalDensity * stepSize * 0.001; // Scale to representative optical depth
}

#endif // HELPERS_CLOUDS_GLSL
