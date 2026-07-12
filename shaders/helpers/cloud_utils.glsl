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

vec3 beerPowder(vec3 d, vec3 local_d) {
	// Approximation of multiple scattering (Beer-Powder law)
	// Ensuring sunny side isn't black when d is small
	return max(
		exp(-d),
		exp(-d * cloudPowderScale) * cloudPowderMultiplier * (vec3(1.0) - exp(-local_d * cloudPowderLocalScale))
	);
}

// https://iquilezles.org/articles/smin
float smin( float a, float b, float k )
{
    float h = max(k-abs(a-b),0.0);
    return min(a, b) - h*h*0.25/k;
}

// --- Configurable Analytical SDF Sphere Parameters ---
// Number of spheres mapped to edges of the tileable square
const int NUM_CL_SPHERES = 4;

// Base local locations of spheres within the unit cell [0, 1]^3
const vec3 CL_SPHERE_CENTERS[4] = vec3[](
	vec3(0.5, 0.5, 0.0),
	vec3(0.5, 0.5, 1.0),
	vec3(1.0, 0.5, 0.5),
	vec3(0.0, 0.5, 0.5)
);

// Base radii of spheres
const float CL_SPHERE_RADII[4] = float[](
	0.35,
	0.35,
	0.35,
	0.35
);

// Easily adjustable variations
const float CL_HEIGHT_VARIATION = 0.15;
const float CL_RADIUS_VARIATION = 0.1;

// Simple inline hash for cell-specific variation
float hash_sdf(vec3 p) {
	return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453123);
}

// Highly configurable, simple and tileable spheres-based SDF
float evalTiledSpheresSdf(vec3 p) {
	vec3 i = floor(p);
	vec3 f = fract(p);
	float d = 1e10;

	for (int s = 0; s < NUM_CL_SPHERES; s++) {
		vec3 center = CL_SPHERE_CENTERS[s];
		float baseRadius = CL_SPHERE_RADII[s];

		// Compute pseudorandom value per-cell per-sphere
		float h = hash_sdf(i + center * 17.0);

		// Adjust height (Y) and size based on variations
		center.y += (h * 2.0 - 1.0) * CL_HEIGHT_VARIATION;
		float radius = baseRadius + (fract(h * 31.39) * 2.0 - 1.0) * CL_RADIUS_VARIATION;

		// Distance to the sphere (handles tiling boundaries cleanly because spheres are close to edges/corners)
		float dist = length(f - center) - radius;

		// Smooth minimum blending
		d = smin(d, dist, 0.2);
	}
	return d;
}

// float evalSdf(vec3 p, float time) {
// 	return evalTiledSpheresSdf(p);
// }


float evalSdf(vec3 p, float time) {
	// Define how massive one repeating grid cell is (e.g., 5000 meters)
	float tileScale = 1000.0 * worldScale;

	// Scale the world position down to unit space [0, 1] for the tile math
	vec3 scaledP = p / tileScale;

	float unitDist = evalTiledSpheresSdf(scaledP);

	// Scale the unit distance back to world-space meters to maintain Lipschitz continuity
	return unitDist * tileScale;
}

struct CloudProperties {
	float altitude;
	float thickness;
	float densityBase;
	float coverage;
	float worldScale;
};

struct CloudWeather {
	float sdf;        // Signed Distance Field (world space)
	float heightMap;  // Altitude variety
	float thickness;  // Thickness variety
	float cellID;     // Per-cell variety
};

struct CloudLayer {
	float baseFloor;
	float baseCeiling;
	float thickness;
};

const float cloudFlow = 3.14;
const float clFlowSpeed = 5.0;
vec3 getCloudWindSpeed(float time) {
	return vec3(0);

	float angle = cloudFlow;
	vec2  flowDir = vec2(cos(angle), sin(angle));
	return vec3(flowDir.x, 0.0, flowDir.y) * clFlowSpeed * worldScale * 10.0;
}

vec3 getCloudAdvectionSpeed(float h, float time) {
	return vec3(0);
	float angle = cloudFlow;
	vec2  flowDir = vec2(cos(angle), sin(angle));

	// Dramatic non-linear shear profile
	float shear = h * h * cloudFlowHeightScale * 1.0;

	vec3 advect = getCloudWindSpeed(time);
	advect.xz += flowDir * shear * worldScale * 10.0;

	return advect;
}

vec3 getCloudWindOffset(float time) {
	return time * getCloudWindSpeed(time);
}

vec3 getCloudAdvectionOffset(float h, float time) {
	return time * getCloudAdvectionSpeed(h, time);
}

CloudWeather loadCloudWeather(vec4 tex) {
	CloudWeather weather;
	weather.sdf = tex.r;
	weather.heightMap = tex.g;
	weather.cellID = tex.b;
	weather.thickness = tex.a;

	return weather;
}

CloudWeather computeCloudWeather(vec3 p, CloudProperties props) {
	vec3 advect = getCloudWindOffset(time);
	vec3 p_advected = p + advect;

	// Use baked weather map. Sampling UV is worldXZ / range.
	// Range is 100,000 * worldScale as defined in the bake shader.
	vec3 uv = p_advected / (5000.0 * props.worldScale);
	// vec4 bakedWeather = textureLod(u_cloudWeatherTexture, uv, 0.0);
	// vec4 bakedWeather = vec4(10000*(distance(fract(uv), vec2(0.5)) - 0.5), 0.5, 0.5, 1.0);
	vec4 bakedWeather = vec4(5000*evalSdf(uv, time), 0.5, 0.5, 1.0);
	return loadCloudWeather(bakedWeather);
}


float calculateAnalyticalDensity(vec3 p, CloudWeather weather, CloudLayer layer, CloudProperties props, float time) {
    float dist = evalSdf(p, time);
	return dist;
}



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


float getCloudCoverageFromSDF(float sdf, float worldScale) {
	// SDF is negative inside the cloud, positive outside.
	// We want coverage to be 1.0 inside and 0.0 outside.
	// Use a smooth transition of 500m * worldScale.
	return 1.0 - smoothstep(-500.0 * worldScale, 500.0 * worldScale, sdf);
}

CloudLayer computeCloudLayer(CloudWeather weather, CloudProperties props) {
	// heightMap (gradual) provides a base altitude variation
	float altitudeShift = weather.heightMap * props.thickness * 2.0;

	float coverage = getCloudCoverageFromSDF(weather.sdf, props.worldScale);

	// thickness (cell-based ID) provides dramatic vertical expansion per cell
	// Tall clouds (cumulonimbus) can be much thicker than base thickness
	float verticalExpansion = mix(1.0, 8.0, weather.thickness * coverage);

	CloudLayer layer;
	layer.baseFloor = (props.altitude * props.worldScale) + altitudeShift * props.worldScale;
	layer.baseCeiling = layer.baseFloor + (props.thickness * verticalExpansion) * props.worldScale;
	layer.thickness = max(layer.baseCeiling - layer.baseFloor, 0.001);
	return layer;
}

float remapClamp(float value, float inMin, float inMax, float outMin, float outMax) {
    float t = clamp((value - inMin) / (inMax - inMin), 0.0, 1.0);
    return mix(outMin, outMax, t);
}

float getDensityHeightGradient(float h, float type) {
	// Typical height profiles for different cloud types
	// Stratus: low and thin
	float stratus = smoothstep(0.0, 0.1, h) * (1.0 - smoothstep(0.15, 0.3, h));
	// Cumulus: medium height, billowy
	float cumulus = smoothstep(0.0, 0.2, h) * (1.0 - smoothstep(0.7, 0.9, h));
	// Cumulonimbus: tall, reaching the top
	float cumulonimbus = smoothstep(0.0, 0.05, h) * (1.0 - smoothstep(0.85, 1.0, h));

	// Interpolate based on cloud type (weather.heightMap)
	float res = mix(stratus, cumulus, smoothstep(0.0, 0.5, type));
	res = mix(res, cumulonimbus, smoothstep(0.5, 1.0, type));
	return res;
}

/**
 * Internal helper to calculate a "puffy" 3D SDF for a cloud point.
 */
float calculatePuffyCloudSDF(vec3 p, CloudWeather weather, CloudLayer layer, float worldScale) {
	// Representative "unit" for the height remapping
	float h_unit = 10000.0 * worldScale;

	// Normalize height relative to cloud center
	float cloudCenter = (layer.baseFloor + layer.thickness) * 0.5;
	float h = (p.y - cloudCenter) / h_unit;

	// Puff factor
	float R = 0.5;

	float x = weather.sdf / h_unit;
	float puffy_x = x + R;
	float d3d = h_unit * (length(vec2(max(0.0, puffy_x), h)) - R + min(0.0, puffy_x));


	return d3d;
}

float calculateLoftedCloudSDF(vec3 p, CloudWeather weather, CloudLayer layer, float worldScale) {
    // 1. The 2D footprint distance (positive outside, negative inside)
    float d_edge = weather.sdf;

    // 2. Calculate the depth inside the cloud boundary
    float depthInside = max(0.0, -d_edge);

    // 3. Map internal depth to vertical height along the +Y axis.
    // 'puffSlope' determines how steep the sides of the cloud are.
    // A slope of 1.0 represents a 45-degree rise from the edge.
    float puffSlope = 1.5;
    float domeHeight = depthInside * puffSlope;

    // Clamp the height to the atmospheric layer's defined maximum thickness
    domeHeight = min(domeHeight, layer.thickness);

    // 4. Define the vertical bounds for this specific XZ column
    // This creates a flat bottom at baseFloor and a domed top.
    float columnCenter = layer.baseFloor + (domeHeight * 0.5);
    float d_vertical = abs(p.y - columnCenter) - (domeHeight * 0.5);

    // 5. Intersect the 2D boundary with the dynamic 1D vertical boundary
    // The exact distance to the boundary is the maximum of the two orthogonal distances.
    float d3d = max(d_edge, d_vertical);

    return d3d;
}

/**
 * Calculate cloud shadow factor for a fragment position.
 * Projects the fragment to the cloud layer and samples the weather map SDF directly.
 */
float calculateCloudShadowFactor(vec3 frag_pos, vec3 L, float intensity) {
	if (intensity <= 0.0) return 1.0;
	if (L.y <= 0.001) return 1.0;

	// Skip if fragment is above the cloud layer
	float cloudCeiling = (cloudAltitude + cloudThickness * 8.0) * worldScale;
	if (frag_pos.y > cloudCeiling)
		return 1.0;

	// Project to the cloud ceiling to find the casting XZ position
	float t = (cloudCeiling - frag_pos.y) / L.y;
	// Since frag_pos.y <= cloudCeiling and L.y > 0, t is always >= 0

	vec3 cloudPos = frag_pos + L * t;

	CloudProperties props;
	props.altitude = cloudAltitude;
	props.thickness = cloudThickness;
	props.densityBase = cloudDensity;
	props.coverage = cloudCoverage;
	props.worldScale = worldScale;

	CloudWeather weather = computeCloudWeather(cloudPos, props);
	CloudLayer layer = computeCloudLayer(weather, props);

	float d3d = calculateLoftedCloudSDF(cloudPos, weather, layer, props.worldScale);

	// Sharpness of the cloud edge in meters
	float penumbra = 100.0 * worldScale;

	// Beer's law approximation for the shadow density
	// We multiply by a factor to make the shadow more prominent
	// And apply a slant factor for longer paths at oblique angles
	float slant = 1.0 / max(0.01, L.y);
	float shadowDepth = max(0.0, -d3d / penumbra) * slant;
	float shadowTerm = exp(-shadowDepth * 8.0);

	return mix(1.0, shadowTerm, intensity);
}

float evaluateCloudShadowDensityAtWorldPos(vec2 worldXZ, float time) {
	CloudProperties props;
	props.altitude = cloudAltitude;
	props.thickness = cloudThickness;
	props.densityBase = cloudDensity;
	props.coverage = cloudCoverage;
	props.worldScale = worldScale;

	vec3  basePos = vec3(worldXZ.x, (props.altitude + props.thickness * 0.5) * props.worldScale, worldXZ.y);
	CloudWeather weather = computeCloudWeather(basePos, props);
	CloudLayer layer = computeCloudLayer(weather, props);

	float d3d = calculateLoftedCloudSDF(basePos, weather, layer, props.worldScale);
	float penumbra = 100.0 * worldScale;
	return max(0.0, -d3d / penumbra) * 4.0;
	// return calculateCloudDensityExpV8(basePos, weather, layer, props, time, true).x;
}

