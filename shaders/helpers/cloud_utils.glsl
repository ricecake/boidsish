#include "textures/cloud.glsl"

#include "lygia/generative/psrdnoise.glsl"
#include "lygia/space/uncenter.glsl"

layout(binding = [[CLOUD_SHADOW_MAP_BINDING]]) uniform sampler2DArray u_cloudShadowTexture;
uniform mat4 u_cloudShadowMatrix;
uniform bool u_useCloudShadowMap;

struct CloudProperties {
	float altitude;
	float thickness;
	float densityBase;
	float coverage;
	float worldScale;
};

struct CloudWeather {
	vec3 p;
	float sdf;// LEGACY NAME
	float coverage;
	float density;
	float heightMap;  // Altitude variety
	float thickness;  // Thickness variety
	float ecentricity;
	float curve;
	float centerDist;
};

struct CloudLayer {
	float baseFloor;
	float baseCeiling;
	float thickness;
};

float schlickGain(float x, float g) {
	g = clamp(g, 0.001, 0.999);
	float absDiff = abs(2.0 * x - 1.0);
	float denominator = g + absDiff * (1.0 - 2.0 * g);
	return 0.5 + ((x - 0.5) * (1.0 - g)) / denominator;
}

float schlickBias(float x, float g) {
	// Guard inputs to safe analytical ranges
	float xx = clamp(x, 0.0, 1.0);
	float gg = clamp(g, 1e-4, 1.0 - 1e-4);

	// Convert bias parameter to Schlick formulation factor
	// Schlick's fast alternative: f(x) = x / ((1/a - 2) * (1.0 - x) + 1.0)
	float k = (1.0 / gg) - 2.0;
	return xx / (k * (1.0 - xx) + 1.0);
}


float getCurvedAltitude(vec3 p) {
	// return p.y;
	float R_earth = 6360.0 * 1000.0 * worldScale;
	vec3 earthCenter = vec3(viewPos.x, -R_earth, viewPos.z);
	return length(p - earthCenter) - R_earth;
}

bool intersectSphereLocal(vec3 ro, vec3 rd, float radius, out float t0, out float t1) {
	float b = dot(ro, rd);
	float rLen = length(ro);
	float c = (rLen - radius) * (rLen + radius);
	float det = b * b - c;
	if (det < 0.0)
		return false;
	det = sqrt(det);
	t0 = -b - det;
	t1 = -b + det;
	return true;
}

bool intersectCloudShell(vec3 ro, vec3 rd, float worldScale, out float t_start, out float t_end) {
	float R_earth = 6360.0 * 1000.0 * worldScale;
	float R_floor = R_earth + (cloudAltitude - 500.0) * worldScale;
	float R_ceiling = R_earth + (cloudAltitude + 2.0 * cloudThickness + 500.0) * worldScale;

	vec3 earthCenter = vec3(viewPos.x, -R_earth, viewPos.z);
	vec3 relRo = ro - earthCenter;

	t_start = 1e10;
	t_end = -1e10;

	float t0, t1;
	if (intersectSphereLocal(relRo, rd, R_ceiling, t0, t1)) {
		t_start = max(0.0, t0);
		t_end = t1;

		if (intersectSphereLocal(relRo, rd, R_floor, t0, t1)) {
			if (t0 < 0.0) {
				t_start = max(t_start, t1);
			} else {
				t_end = min(t_end, t0);
			}
		}
		return t_start < t_end;
	}
	return false;
}

float getCloudRelativeHeight(vec3 p, CloudWeather weather, CloudLayer layer, out float localFloor, out float actualThickness) {
	float altitude = getCurvedAltitude(p);
	float altitudeShift = weather.heightMap * layer.thickness;
	float cellRange = 10000.0 * worldScale;
	// float thicknessTaper = clamp(1.0 - pow(weather.centerDist / (cellRange * 0.5), 2.0), 0.0, 1.0);
	// actualThickness = max(weather.thickness * layer.thickness * thicknessTaper, 10.0 * worldScale);
	actualThickness = max(weather.thickness * layer.thickness, 25.0 * worldScale);
	localFloor = layer.baseFloor + altitudeShift;
	return clamp((altitude - localFloor) / actualThickness, 0.0, 1.0);
}

float getCloudRelativeHeight(vec3 p, CloudWeather weather, CloudLayer layer) {
	float localFloor, actualThickness;
	return getCloudRelativeHeight(p, weather, layer, localFloor, actualThickness);
	// float altitude = getCurvedAltitude(p);
	// float altitudeShift = weather.heightMap * layer.thickness;
	// float actualThickness = weather.thickness * layer.thickness;
	// float localFloor = layer.baseFloor + altitudeShift;
	// return clamp((altitude - localFloor) / max(actualThickness, 0.001), 0.0, 1.0);
}

// float saturate(float value) {
// 	return clamp(value, 0.0, 1.0);
// }

vec4 hash41(float p) {
    vec4 p4 = fract(p * vec4(443.897, 441.423, .0973, .1099));
    p4 += dot(p4, p4.wzxy + 19.19);
    return fract((p4.xxyz + p4.yzzw) * p4.zywx);
}


float remap(float value, float valueMin, float valueMax) {
	return (value - valueMin) / (valueMax - valueMin);
}

float remapClamp(float value, float inMin, float inMax, float outMin, float outMax) {
    float t = clamp((value - inMin) / (inMax - inMin), 0.0, 1.0);
    return mix(outMin, outMax, t);
}

float adjust(float value, float scaly) {
	float f = 1.0 - value;
	float h = 0.4; // adjustable filter

	float a = scaly * (1.0-h) + h;
	return clamp((remap(a, f, f + h)), 0.0, 1.0);
}

float cloudPhase(float cosTheta) {
	// Dual-lobe Henyey-Greenstein for forward and back scattering
	// Blended with a large isotropic component to ensure visibility at all angles
	float hg = mix(henyeyGreenstein(cloudPhaseG1, cosTheta), henyeyGreenstein(cloudPhaseG2, cosTheta), cloudPhaseAlpha);
	return mix(hg, (1.0 / (4.0 * PI)), cloudPhaseIsotropic);
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

vec3 getCloudWindSpeed(float time) {
	float angle = cloudFlowDirection;
	vec2  flowDir = vec2(cos(angle), sin(angle));
	return vec3(flowDir.x, 0.0, flowDir.y) * cloudFlowSpeed * worldScale * 10.0;
}

vec3 getCloudAdvectionSpeed(float h, float time) {
	float angle = cloudFlowDirection;
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

// https://iquilezles.org/articles/smin
float smin( float a, float b, float k )
{
	float h = max(k-abs(a-b),0.0);
	return min(a, b) - h*h*0.25/k;
}

CloudWeather loadCloudWeather(vec3 p, CloudProperties props, vec4 tex) {
	// imageStore(outWeatherMap, pixel, vec4(finalCoverage, distF1InMeters, cellID, density));

	CloudWeather weather;
	weather.p = p;

	// Apply props.coverage as an offset/threshold to the baked coverage map
	weather.coverage = clamp(tex.r + (props.coverage * 2.0 - 1.0), 0.0, 1.0);
	weather.heightMap = tex.g;
	weather.thickness = tex.b;
	weather.density = tex.a * props.densityBase;
	if (props.coverage >= 1.0) {
		weather.coverage = 1.0;
		weather.density = props.densityBase;
	}
	// float thickness = clamp(posNoise(p, mapRange, 5), 0.0, (1.0-cellData.f1_dist)*0.25*uCloudThickness/mapRange);

	// weather.thickness = clamp(weather.thickness, 0, (2500* rawCoverage)/(props.thickness * weather.thickness));
	// weather.thickness = clamp(weather.thickness, 0, weather.coverage);
	weather.thickness = smoothstep(0, weather.thickness, weather.coverage);
	// weather.thickness = schlickBias(weather.coverage, 0.25);
	// weather.density *= smoothstep(0.0, 0.95, weather.thickness);
	weather.density = mix(weather.density, weather.coverage * props.densityBase, 0.4);

	// weather.thickness = mix(weather.thickness, weather.density, 0.8);
	weather.heightMap = mix(weather.heightMap, 0.0, weather.density * 0.9);

	// weather.heightMap = clamp(tex.g, 0.01, 1.0);
	// weather.thickness = clamp(tex.b, 0.01, 1.0);
	// weather.density = clamp(tex.a, 0.01, 1.0);
	// weather.ecentricity = uncenter(psrdnoise(p, vec3(10.0)));
	// weather.curve = uncenter(psrdnoise(p/2.0, vec3(10.0)));
	// weather.centerDist = uncenter(psrdnoise(p/3.0, vec3(10.0)));

	weather.sdf = weather.coverage;

	return weather;
}

CloudWeather computeCloudWeather(vec3 p, CloudProperties props, float lod) {
	vec3 advect = getCloudWindOffset(time);
	vec3 p_advected = p + advect;

	// Use baked weather map. Sampling UV is worldXZ / range.
	// Range is 100,000 * worldScale as defined in the bake shader.
	vec2 uv = p_advected.xz / (100000.0 * props.worldScale);
	vec4 bakedWeather = textureLod(u_cloudWeatherTexture, uv, clamp(lod * 6.0, 0.0, 6.0));
	return loadCloudWeather(p, props, bakedWeather);
}

CloudWeather computeCloudWeather(vec3 p, CloudProperties props) {
	return computeCloudWeather(p, props, 0.0);
}

CloudLayer computeCloudLayer(CloudWeather weather, CloudProperties props) {
	CloudLayer layer;
	layer.baseFloor = props.altitude * props.worldScale;
	layer.thickness = max(props.thickness, 0.001) * props.worldScale;
	// The evaluation volume needs twice the thickness to account for a full height cloud at maximum altitude.
	layer.baseCeiling = layer.baseFloor + 2.0 * layer.thickness;
	return layer;
}

float getDensityHeightGradient(float h, float type) {
	// 1. Cumulonimbus (Type ~ 0.0): Massive storm chunks
	// Solid, dark, flat base with a dense core that tapers slightly near the anvil top.
	float cumulonimbus = smoothstep(0.0, 0.05, h) * (1.0 - smoothstep(0.7, 1.0, h));

	// 2. Cumulus (Type ~ 0.5): Puffy fair-weather clouds
	// Rounded bottoms and very rounded, billowy tops.
	float cumulus = smoothstep(0.0, 0.2, h) * (1.0 - smoothstep(0.6, 0.9, h));

	// 3. Stratus (Type ~ 1.0): Thin, patchy plates
	// Soft fade in, flat middle, soft fade out.
	float stratus = smoothstep(0.0, 0.3, h) * (1.0 - smoothstep(0.7, 1.0, h));

	float res = mix(cumulonimbus, cumulus, smoothstep(0.0, 0.5, type));
	res = mix(res, stratus, smoothstep(0.5, 1.0, type));

	return res;
}

float getCloud3DCoverage(vec3 p, CloudWeather weather, CloudLayer layer, float worldScale) {
	float localFloor, actualThickness;
	float h = getCloudRelativeHeight(p, weather, layer, localFloor, actualThickness);

	if (p.y < localFloor || p.y > (localFloor + actualThickness)) {
		return 0.0;
	}

	float type = weather.heightMap;
	float heightGradient = getDensityHeightGradient(h, type);

	float coverage2D = weather.coverage; //

	// // Create a flare modifier that increases in the upper half of the cloud.
	// // Adjust the smoothstep bounds and multiplier to control the flare's altitude and width.
	// float topFlare = smoothstep(0.4, 0.9, h) * 0.4;

	// // Apply the flare to the 2D coverage, clamping to keep it a valid SDF/mask.
	// float dynamicCoverage = clamp(coverage2D + topFlare, 0.0, 1.0);

	// // Replace the static coverage2D with the dynamically flaring one.
	// float macroVolume = dynamicCoverage * heightGradient; //[cite: 3]

	float macroVolume = coverage2D * heightGradient;
	return macroVolume;
}

float sampleDeepOpacityMap(vec2 shadowUV, float h, float lod) {
	float layerIdx = 8.0 * (1.0 - h) - 1.0;
	if (layerIdx < 0.0) {
		float t = layerIdx + 1.0;
		float depth0 = textureLod(u_cloudShadowTexture, vec3(shadowUV, 0.0), lod).r;
		return mix(0.0, depth0, clamp(t, 0.0, 1.0));
	} else {
		float floorIdx = floor(layerIdx);
		float ceilIdx = ceil(layerIdx);
		float t = fract(layerIdx);
		float depthFloor = textureLod(u_cloudShadowTexture, vec3(shadowUV, clamp(floorIdx, 0.0, 7.0)), lod).r;
		float depthCeil = textureLod(u_cloudShadowTexture, vec3(shadowUV, clamp(ceilIdx, 0.0, 7.0)), lod).r;
		return mix(depthFloor, depthCeil, t);
	}
}

/**
 * Calculate cloud shadow factor for a fragment position using the deep opacity map.
 */
float calculateCloudShadowFactor(vec3 frag_pos, vec3 L, float intensity) {
	if (intensity <= 0.0) return 1.0;
	if (!u_useCloudShadowMap) return 1.0;

	// Project fragment pos to light space to get shadowUV
	vec4 lightSpacePos = u_cloudShadowMatrix * vec4(frag_pos, 1.0);
	vec2 shadowUV = lightSpacePos.xy * 0.5 + 0.5;

	CloudProperties props;
	props.altitude = cloudAltitude;
	props.thickness = cloudThickness;
	props.densityBase = cloudDensity;
	props.coverage = cloudCoverage;
	props.worldScale = worldScale;

	CloudWeather weather = computeCloudWeather(frag_pos, props);
	CloudLayer layer = computeCloudLayer(weather, props);
	float h = getCloudRelativeHeight(frag_pos, weather, layer);

	// Scale accumulated density by a physical factor (0.02) to match average extinction values and keep shadows soft/realistic
	float accumulatedDensity = sampleDeepOpacityMap(shadowUV, h, 0.0) * 0.0075;
	float shadowTerm = exp(-accumulatedDensity);

	return mix(1.0, shadowTerm, intensity);
}

float evaluateCloudShadowDensityAtWorldPos(vec2 worldXZ, float time) {
	if (!u_useCloudShadowMap) return 0.0;
	vec4 lightSpacePos = u_cloudShadowMatrix * vec4(worldXZ.x, 0.0, worldXZ.y, 1.0);
	vec2 shadowUV = lightSpacePos.xy * 0.5 + 0.5;
	// Sample bottom layer (layer 7) for total optical depth through clouds, scaled appropriately
	float totalDensity = textureLod(u_cloudShadowTexture, vec3(shadowUV, 7.0), 0.0).r * 0.0075;
	return totalDensity;
}

/**
 * Calculate local ambient occlusion from clouds at a fragment position using the deep opacity map.
 */
float calculateCloudAmbientOcclusion(vec3 frag_pos) {
	if (!u_useCloudShadowMap) return 1.0;

	// Project fragment pos to light space to get shadowUV
	vec4 lightSpacePos = u_cloudShadowMatrix * vec4(frag_pos, 1.0);
	vec2 shadowUV = lightSpacePos.xy * 0.5 + 0.5;

	CloudProperties props;
	props.altitude = cloudAltitude;
	props.thickness = cloudThickness;
	props.densityBase = cloudDensity;
	props.coverage = cloudCoverage;
	props.worldScale = worldScale;

	CloudWeather weather = computeCloudWeather(frag_pos, props);
	CloudLayer layer = computeCloudLayer(weather, props);
	float h = getCloudRelativeHeight(frag_pos, weather, layer);

	// Scale accumulated density by a physical factor (0.02) to match average extinction values and keep ambient occlusion soft/realistic
	float accumulatedDensity = sampleDeepOpacityMap(shadowUV, h, 6.0) * 0.0075; // high LOD for soft ambient occlusion
	float cloudAO = exp(-accumulatedDensity); // scaled down for softer ambient occlusion

	return mix(1.0, cloudAO, cloudShadowIntensity);
}

