#include "../atmosphere/common.glsl"
#include "textures/cloud.glsl"
#include "math.glsl"

#include "lygia/generative/psrdnoise.glsl"
#include "lygia/space/uncenter.glsl"
#include "lygia/math/mmix.glsl"

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
	float coverage;
	float density;
	float heightMap;  // Altitude variety
	float thickness;  // Thickness variety
	float ridge;      // Ridge noise weighted by Worley F1 distance
	float ecentricity;
	float curve;
	float centerDist;
	float baseFloor;
	float baseCeiling;
	float height;
	float moisture;
	float humidity;
};

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

// https://iquilezles.org/articles/smin
float smin( float a, float b, float k )
{
	float h = max(k-abs(a-b),0.0);
	return min(a, b) - h*h*0.25/k;
}

float smaxCubic(float a, float b, float k) {
	k *= 1.4;
	float h = max(k - abs(a - b), 0.0);
	return max(a, b) + h * h * h / (6.0 * k * k);
}

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

bool intersectCloudShell(vec3 ro, vec3 rd, float worldScale, out float t_start1, out float t_end1, out float t_start2, out float t_end2) {
	float R_earth = 6360.0 * 1000.0 * worldScale;
	float R_floor = R_earth + (cloudAltitude - 500.0) * worldScale;
	float R_ceiling = R_earth + (cloudAltitude + 2.0 * cloudThickness + 500.0) * worldScale;

	vec3 earthCenter = vec3(viewPos.x, -R_earth, viewPos.z);
	vec3 relRo = ro - earthCenter;

	t_start1 = 0.0;
	t_end1 = -1.0;
	t_start2 = 0.0;
	t_end2 = -1.0;

	float t_out0, t_out1;
	if (!intersectSphereLocal(relRo, rd, R_ceiling, t_out0, t_out1) || t_out1 <= 0.0) {
		return false;
	}

	// An intersection with the Earth sphere stops the ray
	float t_e0, t_e1;
	bool hits_earth = intersectSphereLocal(relRo, rd, R_earth, t_e0, t_e1) && (t_e0 > 0.0);
	float t_max = hits_earth ? t_e0 : t_out1;

	float t_c_entry = max(0.0, t_out0);
	float t_c_exit  = min(t_out1, t_max);

	if (t_c_entry >= t_c_exit) {
		return false;
	}

	float t_in0, t_in1;
	if (intersectSphereLocal(relRo, rd, R_floor, t_in0, t_in1)) {
		if (t_in0 < 0.0) {
			// Ray origin is inside R_floor (e.g., ground looking up into the sky)
			float ts = max(t_c_entry, t_in1);
			float te = t_c_exit;
			if (ts < te) {
				t_start1 = ts;
				t_end1 = te;
				return true;
			}
			return false;
		} else {
			// Ray enters R_floor at t_in0 > 0.0
			// Segment 1: from entry into atmosphere ceiling up to entering R_floor
			float ts1 = t_c_entry;
			float te1 = min(t_c_exit, t_in0);
			if (ts1 < te1) {
				t_start1 = ts1;
				t_end1 = te1;
			}

			// Segment 2: from exiting R_floor at t_in1 up to exiting R_ceiling (if not blocked by Earth)
			float ts2 = max(t_c_entry, t_in1);
			float te2 = t_c_exit;
			if (ts2 < te2) {
				if (t_start1 >= t_end1) {
					t_start1 = ts2;
					t_end1 = te2;
				} else {
					t_start2 = ts2;
					t_end2 = te2;
				}
			}

			return (t_start1 < t_end1) || (t_start2 < t_end2);
		}
	} else {
		// Does not intersect R_floor (grazes cloud layer without reaching floor)
		t_start1 = t_c_entry;
		t_end1 = t_c_exit;
		return t_start1 < t_end1;
	}
}

bool intersectCloudShell(vec3 ro, vec3 rd, float worldScale, out float t_start, out float t_end) {
	float t_s1, t_e1, t_s2, t_e2;
	if (intersectCloudShell(ro, rd, worldScale, t_s1, t_e1, t_s2, t_e2)) {
		if (t_s1 < t_e1) {
			t_start = t_s1;
			t_end = t_e1;
			return true;
		} else if (t_s2 < t_e2) {
			t_start = t_s2;
			t_end = t_e2;
			return true;
		}
	}
	t_start = 1e10;
	t_end = -1e10;
	return false;
}

float getCurvedAltitude(vec3 p) {
	// return p.y;
	float R_earth = 6360.0 * 1000.0 * worldScale;
	vec3 earthCenter = vec3(viewPos.x, -R_earth, viewPos.z);
	return length(p - earthCenter) - R_earth;
}

float getCloudRelativeHeight(vec3 p, CloudWeather weather, out float localFloor, out float actualThickness) {
	float altitude = getCurvedAltitude(p);
	float altitudeShift = weather.heightMap * weather.height;

	actualThickness = max(weather.thickness * weather.height, 25.0 * worldScale);
	localFloor = weather.baseFloor + altitudeShift;
	return clamp((altitude - localFloor) / actualThickness, 0.0, 1.0);
}

float getCloudRelativeHeight(vec3 p, CloudWeather weather) {
	float localFloor, actualThickness;
	return getCloudRelativeHeight(p, weather, localFloor, actualThickness);
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


float applyDynamicCoverage(float bakedCoverage, float uniformCoverage, float biasTune) {
    float coverageFloor = 1.0 - (uniformCoverage * 2.0);
    float remapped = saturate((bakedCoverage - coverageFloor) / (1.0 - min(0.0, coverageFloor)));

    // biasTune > 0.5 = ease-out, biasTune < 0.5 = ease-in
    float t = clamp(biasTune, 0.001, 0.999);
    return remapped / (((1.0 / t) - 2.0) * (1.0 - remapped) + 1.0);
}

float applyDynamicCoverage(float bakedCoverage, float uniformCoverage) {
	// uniformCoverage = 1.0 - uniformCoverage;
	// return smoothstep(uniformCoverage, uniformCoverage + 0.1, bakedCoverage);
    float coverageFloor = 1.0 - (uniformCoverage * 2.0);

    float remapped = saturate((bakedCoverage - coverageFloor) / (1.0 - min(0.0, coverageFloor)));

	return schlickGain(remapped, 0.25);
}


// float applyDynamicCoverage(float bakedCoverage, float uniformCoverage) {
//     // uniformCoverage at 0.5 = no change
//     // uniformCoverage at 1.0 = solid overcast
//     // uniformCoverage at 0.0 = clear skies
// 	// return 1.0-smoothstep(2*uniformCoverage-1,2*uniformCoverage,bakedCoverage);
//     // return clamp(bakedCoverage + (uniformCoverage * 2.0 - 1.0), 0.0, 1.0);
// 	// return 6*(bakedCoverage+(uniformCoverage*2.0 -1));
// 	// return 1.0-smoothstep(uniformCoverage-0.25, uniformCoverage, bakedCoverage - (uniformCoverage * 0.25));
// 	// return 1.0-smoothstep(uniformCoverage-0.5*uniformCoverage, uniformCoverage, bakedCoverage - (uniformCoverage * uniformCoverage * 0.5));
// 	// return schlickBias(clamp(bakedCoverage + (uniformCoverage * 2.0 - 1.0), 0.0, 1.0), uniformCoverage);
// 	// return schlickBias(bakedCoverage, uniformCoverage);
// 	return applyDynamicCoverage(bakedCoverage, uniformCoverage, 1-uniformCoverage);
// }


CloudWeather loadCloudWeather(vec3 p, CloudProperties props, vec4 tex, vec4 frontSample) {
	CloudWeather weather;
	weather.p = p;

	float baseCoverage = tex.r;
	float frontCoverageBoost = frontSample.g;
	weather.coverage = applyDynamicCoverage(baseCoverage * frontCoverageBoost, props.coverage);

	float bakedType = mmix(0.05, 0.0, 0.75, tex.g);
	weather.heightMap = mix(bakedType, frontSample.r, 0.5);

	float frontThicknessMod = frontSample.b;
	weather.thickness = mmix(0.15, 1.0, 0.05, tex.g) * frontThicknessMod;
	weather.density = tex.a * props.densityBase * frontCoverageBoost;
	weather.ridge = tex.b;
	weather.moisture = tex.a;
	weather.humidity = frontSample.b;

	weather.ecentricity = frontSample.a;

	weather.baseFloor = props.altitude * props.worldScale;
	weather.height = max(props.thickness * frontThicknessMod, 0.001) * props.worldScale;
	weather.baseCeiling = weather.baseFloor + 2.0 * weather.height;

	return weather;
}

CloudWeather loadCloudWeather(vec3 p, CloudProperties props, vec4 tex) {
	return loadCloudWeather(p, props, tex, vec4(0.5, 1.0, 1.0, 1.0));
}

CloudWeather computeCloudWeather(vec3 p, CloudProperties props, float lod) {
	vec3 advect = getCloudWindOffset(time);
	vec3 p_advected = p + advect;

	// Use baked weather map. Sampling UV is worldXZ / range.
	// Range is 100,000 * worldScale as defined in the bake shader.
	vec2 uv = p_advected.xz / (100000.0 * props.worldScale);
	float texelWorldSize = (100000.0 * props.worldScale) / max(1.0, float(textureSize(u_cloudWeatherTexture, 0).x));
	float weatherMip = calculatePixelWorldSizeToTextureLod(lod, texelWorldSize);
	vec4 bakedWeather = textureLod(u_cloudWeatherTexture, uv, clamp(weatherMip, 0.0, 11.0));

	// Evaluate 3D cloud front lookup texture using local weather state (wind speed, temperature, humidity)
	vec2 weatherUV;
	if (u_windOriginSize.y > 0) {
		float gridSpacing = u_windParams.x;
		vec2 gridCoord = (p.xz / gridSpacing) - vec2(u_windOriginSize.xz);
		weatherUV = gridCoord / vec2(u_windOriginSize.y, u_windOriginSize.w);
	} else {
		float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;
		weatherUV = (p.xz / scaledChunkSize - vec2(u_originSize.xy)) / 128.0;
	}

	vec4 scalars = textureLod(u_weatherScalars, weatherUV, 0.0);
	vec4 macroWind = textureLod(u_lbmWindTexture, weatherUV, 0.0);

	float normWind = clamp(length(macroWind.xz) / 30.0, 0.0, 1.0);
	float normTemp = clamp((scalars.x - 250.0) / 70.0, 0.0, 1.0);
	float normHum = clamp(scalars.y, 0.0, 1.0);

	vec4 frontSample = textureLod(u_cloud3DFrontLUT, vec3(normWind, normTemp, normHum), 0.0);

	return loadCloudWeather(p, props, bakedWeather, frontSample);
}

CloudWeather computeCloudWeather(vec3 p, CloudProperties props) {
	return computeCloudWeather(p, props, 0.0);
}

float getDensityHeightGradient(float h, float type) {
	return textureLod(u_cloud2DPropsLUT, vec2(clamp(type, 0.0, 1.0), clamp(h, 0.0, 1.0)), 0.0).r;
}

/**
 * Calculate the approximate distance from the current point inside the cloud to the nearest cloud edge.
 * Uses the coverage map value, the height within the cloud, and the cloud profile function.
 */
float getDistanceToCloudEdge(float coverage, float h, float type, float thickness) {
	// Evaluate the cloud profile function at this height
	float heightGradient = getDensityHeightGradient(h, type);

	// Horizontal distance: we are deep inside if coverage is 1.0, at the edge if 0.0.
	// Scale by a characteristic horizontal scale (e.g., twice the thickness).
	float horizontalScale = max(thickness * 2.0, 2000.0);
	float distHorizontal = coverage * horizontalScale;

	// Vertical distance: physically, it is min(h, 1.0 - h) * thickness.
	// We modulate this by the height gradient to account for vertical tapering/shaping.
	float distVertical = min(h, 1.0 - h) * thickness * heightGradient;

	// The overall distance to the nearest edge is the minimum of horizontal and vertical distance.
	return max(0.0, min(distHorizontal, distVertical));
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
	float h = getCloudRelativeHeight(frag_pos, weather);

	// Scale accumulated density by a physical factor (0.02) to match average extinction values and keep shadows soft/realistic
	float accumulatedDensity = sampleDeepOpacityMap(shadowUV, h, 0.0) * 0.001 * cloudShadowOpticalDepthMultiplier / max(0.001, worldScale);
	float shadowTerm = exp(-accumulatedDensity);

	return mix(1.0, shadowTerm, intensity);
}

/**
 * Calculate local ambient occlusion from clouds at a fragment position using the deep opacity map.
 */
float calculateCloudAmbientOcclusion(vec3 frag_pos) {
	if (!u_useCloudShadowMap) return 1.0;

	CloudProperties props;
	props.altitude = cloudAltitude;
	props.thickness = cloudThickness;
	props.densityBase = cloudDensity;
	props.coverage = cloudCoverage;
	props.worldScale = worldScale;

	CloudWeather weather = computeCloudWeather(frag_pos, props);

	float localFloor, actualThickness;
	float h = getCloudRelativeHeight(frag_pos, weather, localFloor, actualThickness);

	// Find the curved altitude directly above the fragment clamped within the cloud layer
	float alt_above = clamp(getCurvedAltitude(frag_pos), localFloor, localFloor + actualThickness);

	// Convert curved altitude back to Cartesian coordinates for the point directly above frag_pos
	float R_earth = 6360.0 * 1000.0 * worldScale;
	float distXZ_sq = dot(frag_pos.xz - viewPos.xz, frag_pos.xz - viewPos.xz);
	float rad_above = alt_above + R_earth;
	float y_above = sqrt(max(0.0, rad_above * rad_above - distXZ_sq)) - R_earth;
	vec3 P_above = vec3(frag_pos.x, y_above, frag_pos.z);

	// Project P_above to light space to get the correct shadowUV directly above the point
	vec4 lightSpacePos_above = u_cloudShadowMatrix * vec4(P_above, 1.0);
	vec2 shadowUV_above = lightSpacePos_above.xy * 0.5 + 0.5;

	// Scale accumulated density by a physical factor (0.02) to match average extinction values and keep ambient occlusion soft/realistic
	float accumulatedDensity = sampleDeepOpacityMap(shadowUV_above, h, 1.0) * 0.001 * cloudShadowOpticalDepthMultiplier / max(0.001, worldScale); // high LOD for soft ambient occlusion
	float cloudAO = exp(-accumulatedDensity); // scaled down for softer ambient occlusion

	return mix(1.0, cloudAO, cloudShadowIntensity);
}

