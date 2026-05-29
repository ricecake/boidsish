#ifndef GSHADERS_HELPERS_LIGHTING_GLSL
#define GSHADERS_HELPERS_LIGHTING_GLSL
#ifndef HELPERS_LIGHTING_GLSL
#define HELPERS_LIGHTING_GLSL

//START shaders/helpers/../helpers/constants.glsl
#ifndef GSHADERS_HELPERS_CONSTANTS_GLSL
#define GSHADERS_HELPERS_CONSTANTS_GLSL
#ifndef CONSTANTS
#define CONSTANTS

const float PI = 3.14159265359;

#endif
#endif // GSHADERS_HELPERS_CONSTANTS_GLSL
//END shaders/helpers/../helpers/constants.glsl (returning to shaders/helpers/lighting.glsl)
//START shaders/helpers/../lighting.glsl
#ifndef GSHADERS_LIGHTING_GLSL
#define GSHADERS_LIGHTING_GLSL
#ifndef LIGHTING_GLSL
#define LIGHTING_GLSL

//START shaders/types/lighting.glsl
#ifndef GSHADERS_TYPES_LIGHTING_GLSL
#define GSHADERS_TYPES_LIGHTING_GLSL
#ifndef LIGHTING_TYPES_GLSL
#define LIGHTING_TYPES_GLSL

struct Light {
	vec3  position;
	float intensity;
	vec3  color;
	int   type;
	vec3  direction;
	float inner_cutoff; // Also: emissive_radius (EMISSIVE), flash_radius (FLASH)
	float outer_cutoff; // Also: falloff_exp (FLASH)
};

const int MAX_LIGHTS = 10;

layout(std140, binding = 0) uniform Lighting {
	Light lights[MAX_LIGHTS];
	int   num_lights;
	float worldScale;
	float dayTime;
	float nightFactor;
	vec3  viewPos;
	float cloudShadowIntensity;
	vec3  ambient_light;
	float time;
	vec3  viewDir;
	float cloudAltitude;
	float cloudThickness;
	float cloudDensity;
	float cloudCoverage;
	float cloudWarp;
	float cloudPhaseG1;
	float cloudPhaseG2;
	float cloudPhaseAlpha;
	float cloudPhaseIsotropic;
	float cloudPowderScale;
	float cloudPowderMultiplier;
	float cloudPowderLocalScale;
	float cloudShadowOpticalDepthMultiplier;
	float cloudShadowStepMultiplier;
	float cloudSunLightScale;
	float cloudMoonLightScale;
	float cloudBeerPowderMix;
	mat4  cloudShadowMatrix;
	vec3  lightningColor;
	float lightningPulse;
	vec4  sh_coeffs[9];
	vec4  u_probeParams;   // origin_x, origin_z, size_x, size_z
	vec4  u_probeParams2;  // size_y, spacing, oct_res
};

#endif
#endif // GSHADERS_TYPES_LIGHTING_GLSL
//END shaders/types/lighting.glsl (returning to shaders/lighting.glsl)
//START shaders/types/terrain.glsl
#ifndef GSHADERS_TYPES_TERRAIN_GLSL
#define GSHADERS_TYPES_TERRAIN_GLSL
#ifndef TERRAIN_TYPES_GLSL
#define TERRAIN_TYPES_GLSL

struct AmbientProbe {
	vec4 sh_coeffs[9]; // rgb = coefficients, w = unused
};

#ifndef TERRAIN_DATA_BLOCK
#define TERRAIN_DATA_BLOCK
layout(std140, binding = 8) uniform TerrainData {
	ivec4 u_originSize;    // x, z, size, is_bound
	vec4  u_terrainParams; // chunkSize, worldScale
	vec4  u_probeParams_unused;   // origin_x, origin_z, size_x, size_z
	vec4  u_probeParams2_unused;  // size_y, spacing, oct_res
};
#endif

#ifndef TERRAIN_PROBES_BLOCK
#define TERRAIN_PROBES_BLOCK
layout(std430, binding = 29) buffer TerrainProbes {
	AmbientProbe u_terrainProbes[];
};
#endif

#endif
#endif // GSHADERS_TYPES_TERRAIN_GLSL
//END shaders/types/terrain.glsl (returning to shaders/lighting.glsl)
//START shaders/types/biomes.glsl
#ifndef GSHADERS_TYPES_BIOMES_GLSL
#define GSHADERS_TYPES_BIOMES_GLSL
#ifndef BIOME_TYPES_GLSL
#define BIOME_TYPES_GLSL

struct BiomeShaderProperties {
	vec4 albedo_roughness; // rgb = albedo, w = roughness
	vec4 params;           // x = metallic, y = detailStrength, z = detailScale, w = noiseType
};

#ifndef BIOME_DATA_BLOCK
#define BIOME_DATA_BLOCK
layout(std140, binding = 7) uniform BiomeData {
	BiomeShaderProperties u_biomes[8];
};
#endif

#endif
#endif // GSHADERS_TYPES_BIOMES_GLSL
//END shaders/types/biomes.glsl (returning to shaders/lighting.glsl)
//START shaders/types/shadows.glsl
#ifndef GSHADERS_TYPES_SHADOWS_GLSL
#define GSHADERS_TYPES_SHADOWS_GLSL
#ifndef SHADOW_TYPES_GLSL
#define SHADOW_TYPES_GLSL

const int MAX_SHADOW_MAPS = 16;
const int MAX_CASCADES = 4;

// Shadow mapping UBO (binding set via glUniformBlockBinding to point 2)
layout(std140, binding = 2) uniform Shadows {
	mat4 lightSpaceMatrices[MAX_SHADOW_MAPS];
	vec4 cascadeSplits;
	int  numShadowLights;
};

// Per-light shadow map index (-1 if no shadow for this light)
// This is set via uniform since the Light struct can't easily store it
uniform int lightShadowIndices[10];

#endif
#endif // GSHADERS_TYPES_SHADOWS_GLSL
//END shaders/types/shadows.glsl (returning to shaders/lighting.glsl)
//START shaders/textures/shadows.glsl
#ifndef GSHADERS_TEXTURES_SHADOWS_GLSL
#define GSHADERS_TEXTURES_SHADOWS_GLSL
#ifndef SHADOW_TEXTURES_GLSL
#define SHADOW_TEXTURES_GLSL

//START shaders/types/shadows.glsl
//END shaders/types/shadows.glsl (returning to shaders/textures/shadows.glsl)

// Shadow map texture array - bound to texture unit 4
uniform sampler2DArrayShadow shadowMaps;

#endif
#endif // GSHADERS_TEXTURES_SHADOWS_GLSL
//END shaders/textures/shadows.glsl (returning to shaders/lighting.glsl)

#endif
#endif // GSHADERS_LIGHTING_GLSL
//END shaders/helpers/../lighting.glsl (returning to shaders/helpers/lighting.glsl)
//START shaders/helpers/clouds.glsl
#ifndef GSHADERS_HELPERS_CLOUDS_GLSL
#define GSHADERS_HELPERS_CLOUDS_GLSL
#ifndef HELPERS_CLOUDS_GLSL
#define HELPERS_CLOUDS_GLSL

//START shaders/helpers/../lighting.glsl
//END shaders/helpers/../lighting.glsl (returning to shaders/helpers/clouds.glsl)
//START shaders/helpers/fast_noise.glsl
#ifndef GSHADERS_HELPERS_FAST_NOISE_GLSL
#define GSHADERS_HELPERS_FAST_NOISE_GLSL
#ifndef HELPERS_FAST_NOISE_GLSL
#define HELPERS_FAST_NOISE_GLSL

// Helper functions for fast texture-based noise lookups
// Requires noise texture samplers bound to fixed units:
// u_noiseTexture: 3D, unit 5, R=Simplex/G=Worley/B=FBM/A=Warped
// u_curlTexture: 3D, unit 6, RGB=Curl/A=FBM Curl Mag
// u_blueNoiseTexture: 2D, unit 7, RGBA tiling blue noise at 4 frequencies
// u_extraNoiseTexture: 3D, unit 8, R=Ridge/G=Gradient

#ifndef NOISE_TEXTURES_DEFINED
#define NOISE_TEXTURES_DEFINED
uniform sampler3D u_noiseTexture;
uniform sampler3D u_curlTexture;
uniform sampler2D u_blueNoiseTexture;
uniform sampler3D u_extraNoiseTexture;
uniform sampler2D u_phasorTexture;
#endif

// R: Simplex 3D
float fastSimplex3d(vec3 p) {
	return textureLod(u_noiseTexture, p, 0.0).r * 2.0 - 1.0;
}

// G: Worley 3D
float fastWorley3d(vec3 p) {
	return textureLod(u_noiseTexture, p, 0.0).g;
}

// B: FBM 3D
float fastFbm3d(vec3 p) {
	return textureLod(u_noiseTexture, p, 0.0).b * 2.0 - 1.0;
}

// A: Warped FBM 3D
float fastWarpedFbm3d(vec3 p) {
	return textureLod(u_noiseTexture, p, 0.0).a * 2.0 - 1.0;
}

// Extra Noises (from u_extraNoiseTexture)
// R: Ridge 3D
float fastRidge3d(vec3 p) {
	return textureLod(u_extraNoiseTexture, p, 0.0).r;
}

// G: Gradient 3D
float fastGradient3d(vec3 p) {
	return textureLod(u_extraNoiseTexture, p, 0.0).g * 2.0 - 1.0;
}

vec2 fastWorley3dID(vec3 p) {
	return textureLod(u_extraNoiseTexture, p, 0.0).ba;
}


// Multi-octave texture FBM
float fastTextureFbm(vec3 p, int octaves) {
	float value = 0.0;
	float amplitude = 0.5;
	for (int i = 0; i < octaves; i++) {
		value += amplitude * (textureLod(u_noiseTexture, p, 0.0).r * 2.0 - 1.0);
		p *= 2.0;
		amplitude *= 0.5;
	}
	return value;
}

// Curl Noise lookup
vec3 fastCurl3d(vec3 p) {
	return textureLod(u_curlTexture, p, 0.0).rgb;
}

// FBM Curl magnitude lookup
float fastFbmCurl3d(vec3 p) {
	return textureLod(u_curlTexture, p, 0.0).a;
}

// Blue Noise lookups (at different frequencies)
float fastBlueNoise(vec2 uv, int frequencyIndex) {
	vec4 bn = textureLod(u_blueNoiseTexture, uv, 0.0);
	if (frequencyIndex == 0)
		return bn.r;
	if (frequencyIndex == 1)
		return bn.g;
	if (frequencyIndex == 2)
		return bn.b;
	return bn.a;
}

float fastBlueNoise(vec2 uv) {
	return textureLod(u_blueNoiseTexture, uv, 0.0).r;
}

// Spatiotemporal Blue Noise lookup using golden ratio shift
// Useful for Monte Carlo integration across frames
float fastSpatiotemporalBlueNoise(vec2 uv, int frequencyIndex, int frameIndex) {
    float bn = fastBlueNoise(uv, frequencyIndex);
    // Golden ratio = 0.61803398875
    return fract(bn + float(frameIndex) * 0.61803398875);
}

// float fastSpatiotemporalBlueNoise(vec2 uv, int frameIndex) {
// 	return fastSpatiotemporalBlueNoise(uv, 0, frameIndex);
// }

vec4 fastSpatiotemporalBlueNoise(vec2 uv, int frameIndex) {
	ivec2 bnSize = textureSize(u_blueNoiseTexture, 0);
	vec2 bnUV = (uv + vec2(frameIndex * 13, frameIndex * 7)) / vec2(bnSize);
	vec4 bn = textureLod(u_blueNoiseTexture, bnUV, 0.0);
    // float bn = fastBlueNoise(uv, frequencyIndex);
    // Golden ratio = 0.61803398875
    // return fract(bn + vec4(2.0*sin(frameIndex*0.5)  * 0.61803398875));
    return fract(bn + vec4(frameIndex)  * 0.61803398875);
}

/**
 * Fast 2D Phasor noise lookup.
 * Performs complex multiplication of baked phasor with runtime phase.
 * Returns the real part of the resulting complex number.
 */
float fastPhasor2d(vec2 uv, float runtimePhase) {
	vec2 baked = textureLod(u_phasorTexture, uv, 0.0).rg;

	// Complex multiplication: (R_baked + i*I_baked) * (cos(phi) + i*sin(phi))
	// Result real part = R_baked * cos(phi) - I_baked * sin(phi)
	float cosPhi = cos(runtimePhase);
	float sinPhi = sin(runtimePhase);

	return baked.x * cosPhi - baked.y * sinPhi;
}

#endif // HELPERS_FAST_NOISE_GLSL
#endif // GSHADERS_HELPERS_FAST_NOISE_GLSL
//END shaders/helpers/fast_noise.glsl (returning to shaders/helpers/clouds.glsl)
//START shaders/helpers/math.glsl
#ifndef GSHADERS_HELPERS_MATH_GLSL
#define GSHADERS_HELPERS_MATH_GLSL

//START shaders/helpers/constants.glsl
//END shaders/helpers/constants.glsl (returning to shaders/helpers/math.glsl)

float roundToEvenPlaces(float value, float places) {
	float shift = pow(10.0, places);
	return roundEven(value * shift) / shift;
}

float roundToPlaces(float value, float places) {
	float shift = pow(10.0, places);
	return round(value * shift) / shift;
}

float henyeyGreenstein(float g, float cosTheta) {
	float g2 = g * g;
	return (1.0 - g2) / (4.0 * PI * pow(max(0.0001, 1.0 + g2 - 2.0 * g * cosTheta), 1.5));
}

float remap(float value, float low1, float high1, float low2, float high2) {
	return low2 + (value - low1) * (high2 - low2) / max(0.0001, (high1 - low1));
}
#endif // GSHADERS_HELPERS_MATH_GLSL
//END shaders/helpers/math.glsl (returning to shaders/helpers/clouds.glsl)

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
		// Include base Worley noise so shadow patterns match the full cloud shapes
		float baseNoise = fastWorley3d(p / (50000.0 * props.worldScale) + time * 0.0005);
		float baseDensity = baseNoise * weather.weatherMap;
		return smoothstep(coverageThreshold, coverageThreshold + 0.4, baseDensity) * densityProfile * props.densityBase;
	}

	// Base noise for cloud shapes
	vec3 p_warped = p + 5.0 * fastCurl3d(p / (900.0 * props.worldScale) + time * 0.002);
	vec3 p_scaled = p_warped / (50000.0 * props.worldScale);

	float baseNoise = fastWorley3d(p_scaled + time * 0.0005);

	// Implement "Roll": Billowy edges that vary with height
	// We remap the base noise threshold based on the vertical position
	float rollFactor = remap(h, 0.0, 1.0, 0.4, 0.1);
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

	CloudLayer layer = computeCloudLayer(weather, props);

	// Sample at the center of the dynamic layer
	cloudPos.y = (layer.baseFloor + layer.baseCeiling) * 0.5;

	return calculateCloudShadowDensity(cloudPos, weather, layer, props, time);
}

#endif // HELPERS_CLOUDS_GLSL
#endif // GSHADERS_HELPERS_CLOUDS_GLSL
//END shaders/helpers/clouds.glsl (returning to shaders/helpers/lighting.glsl)
//START shaders/helpers/brdf.glsl
#ifndef GSHADERS_HELPERS_BRDF_GLSL
#define GSHADERS_HELPERS_BRDF_GLSL
#ifndef HELPERS_BRDF_GLSL
#define HELPERS_BRDF_GLSL

//START shaders/helpers/constants.glsl
//END shaders/helpers/constants.glsl (returning to shaders/helpers/brdf.glsl)

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float r = max(roughness, 0.04);
    float a = r * r;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, 0.0001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / max(denom, 0.0001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickFast(float cosTheta, vec3 F0) {
    float c = clamp(cosTheta, 0.0, 1.0);
    return F0 + (1.0 - F0) * exp2((-5.55473 * c - 6.98316) * c);
}

float VisibilitySmithGGXCorrelated(float NdotL, float NdotV, float roughness) {
    float a2 = roughness * roughness;
    float lambdaV = NdotL * sqrt((NdotV - a2 * NdotV) * NdotV + a2);
    float lambdaL = NdotV * sqrt((NdotL - a2 * NdotL) * NdotL + a2);
    return 0.5 / max(lambdaV + lambdaL, 0.0001);
}

#endif
#endif // GSHADERS_HELPERS_BRDF_GLSL
//END shaders/helpers/brdf.glsl (returning to shaders/helpers/lighting.glsl)
//START shaders/helpers/octahedral.glsl
#ifndef GSHADERS_HELPERS_OCTAHEDRAL_GLSL
#define GSHADERS_HELPERS_OCTAHEDRAL_GLSL
#ifndef HELPERS_OCTAHEDRAL_GLSL
#define HELPERS_OCTAHEDRAL_GLSL

/**
 * Wraps octahedral coordinates for the bottom hemisphere.
 */
vec2 octWrap(vec2 v) {
    return (1.0 - abs(v.yx)) * (vec2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0));
}

/**
 * Encodes a unit vector into a 2D octahedral coordinate [0, 1].
 */
vec2 octEncode(vec3 n) {
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    n.xy = n.z >= 0.0 ? n.xy : octWrap(n.xy);
    return n.xy * 0.5 + 0.5;
}

/**
 * Decodes a 2D octahedral coordinate [0, 1] into a unit vector.
 */
vec3 octDecode(vec2 f) {
    f = f * 2.0 - 1.0;
    // https://twitter.com/Stubbesaurus/status/406798031268802560
    vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = clamp(-n.z, 0.0, 1.0);
    n.x += n.x >= 0.0 ? -t : t;
    n.y += n.y >= 0.0 ? -t : t;
    return normalize(n);
}

#endif // HELPERS_OCTAHEDRAL_GLSL
#endif // GSHADERS_HELPERS_OCTAHEDRAL_GLSL
//END shaders/helpers/octahedral.glsl (returning to shaders/helpers/lighting.glsl)

// Atmosphere constants for transmittance lookup
const float kEarthRadiusKM = 6360.0;

#ifndef ATMOSPHERE_HEIGHT_DEFINED
	#define ATMOSPHERE_HEIGHT_DEFINED
uniform float u_atmosphereHeight; // usually 100.0 km
#endif

#ifndef TRANSMITTANCE_LUT_DEFINED
	#define TRANSMITTANCE_LUT_DEFINED
layout(binding = 21) uniform sampler2D u_transmittanceLUT;
#endif

layout(binding = 25) uniform sampler2D u_cloudShadowMap;
#ifndef TERRAIN_GRID_DEFINED
	#define TERRAIN_GRID_DEFINED
layout(binding = 11) uniform isampler2D u_chunkGrid;
#endif

layout(binding = 45) uniform sampler3D u_probeIrradiance;
layout(binding = 46) uniform sampler3D u_probeDepth;

/**
 * Maps height and sun cosine angle to UV coordinates for the transmittance LUT.
 * Matches logic in atmosphere/common.glsl but standalone here for convenience.
 */
vec2 getTransmittanceUV(float r, float mu) {
	float x_mu = mu * 0.5 + 0.5;
	float x_r = (r - kEarthRadiusKM) / max(u_atmosphereHeight, 1.0);
	return vec2(clamp(x_mu, 0.0, 1.0), clamp(x_r, 0.0, 1.0));
}

const int LIGHT_TYPE_POINT = 0;
const int LIGHT_TYPE_DIRECTIONAL = 1;
const int LIGHT_TYPE_SPOT = 2;
const int LIGHT_TYPE_EMISSIVE = 3; // Glowing object light (can cast shadows)
const int LIGHT_TYPE_FLASH = 4;    // Explosion/flash light (rapid falloff)

/**
 * Calculate light direction and attenuation for any light type.
 * @param light_index Index into the lights array
 * @param frag_pos Fragment world position
 * @param light_dir Output: normalized direction from fragment to light
 * @param attenuation Output: combined distance and angular attenuation
 */
void calculateLightContribution(int light_index, vec3 frag_pos, out vec3 light_dir, out float attenuation) {
	attenuation = 1.0;

	if (lights[light_index].type == LIGHT_TYPE_POINT) {
		// Point light: attenuates with distance
		light_dir = normalize(lights[light_index].position - frag_pos);
		float distance = length(lights[light_index].position - frag_pos);
		// Practical attenuation curve (inverse square falloff with linear term)
		attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);

	} else if (lights[light_index].type == LIGHT_TYPE_DIRECTIONAL) {
		// Directional light: no attenuation, parallel rays
		light_dir = normalize(-lights[light_index].direction);
		attenuation = 1.0;

	} else if (lights[light_index].type == LIGHT_TYPE_SPOT) {
		// Spot light: distance attenuation + angular falloff
		light_dir = normalize(lights[light_index].position - frag_pos);
		float distance = length(lights[light_index].position - frag_pos);
		attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);

		// Angular falloff using inner/outer cutoff angles
		float theta = dot(light_dir, normalize(-lights[light_index].direction));
		float epsilon = lights[light_index].inner_cutoff - lights[light_index].outer_cutoff;
		float angular_intensity = clamp((theta - lights[light_index].outer_cutoff) / epsilon, 0.0, 1.0);
		attenuation *= angular_intensity;

	} else if (lights[light_index].type == LIGHT_TYPE_EMISSIVE) {
		// Emissive/glowing object light: similar to point light but with soft near-field
		// inner_cutoff stores the emissive object radius for soft falloff
		light_dir = normalize(lights[light_index].position - frag_pos);
		float distance = length(lights[light_index].position - frag_pos);
		float emissive_radius = lights[light_index].inner_cutoff;

		// Soft falloff that accounts for the size of the glowing object
		// Avoids harsh falloff when very close to the light source
		float effective_dist = max(distance - emissive_radius * 0.5, 0.0);
		attenuation = 1.0 / (1.0 + 0.09 * effective_dist + 0.032 * effective_dist * effective_dist);

		// Boost intensity when inside or near the emissive radius
		float proximity_boost = smoothstep(emissive_radius * 2.0, 0.0, distance);
		attenuation = mix(attenuation, 1.0, proximity_boost * 0.5);

	} else if (lights[light_index].type == LIGHT_TYPE_FLASH) {
		// Flash/explosion light: very bright with rapid falloff
		// inner_cutoff = flash radius, outer_cutoff = falloff exponent
		light_dir = normalize(lights[light_index].position - frag_pos);
		float distance = length(lights[light_index].position - frag_pos);
		float flash_radius = lights[light_index].inner_cutoff;
		float falloff_exp = lights[light_index].outer_cutoff;

		// Normalized distance (0 at center, 1 at radius edge)
		float norm_dist = distance / max(flash_radius, 0.001);

		// Sharp inverse-power falloff for explosive effect
		// Falls off rapidly but smoothly
		attenuation = 1.0 / pow(1.0 + norm_dist, falloff_exp);

		// Hard cutoff at 2x radius to prevent distant influence
		attenuation *= smoothstep(2.0, 1.5, norm_dist);
	}
}

/**
 * Calculate cloud shadow factor for a fragment position.
 * Projects the fragment position to the cloud layer along the light direction.
 */
float calculateCloudShadow(int light_index, vec3 frag_pos) {
	if (lights[light_index].type != LIGHT_TYPE_DIRECTIONAL || cloudShadowIntensity <= 0.0) {
		return 1.0;
	}

	vec3 L = normalize(-lights[light_index].direction);
	if (L.y <= 0.0)
		return 1.0;

	// Project to the middle of the cloud layer to ensure we're within the tapering range
	float shadowAltitude = cloudAltitude + cloudThickness * 0.5;
	float scaledCloudAltitude = shadowAltitude * worldScale;
	float t = (scaledCloudAltitude - frag_pos.y) / L.y;

	if (t < 0.0)
		return 1.0;

	vec3 cloudPos = frag_pos + L * t;

	// Use the precomputed 2D shadow map
	vec4 shadowUV = cloudShadowMatrix * vec4(cloudPos.xz, 0.0, 1.0);

	// Sample the shadow map
	float d = 0.0;
	if (shadowUV.x >= 0.0 && shadowUV.x <= 1.0 && shadowUV.y >= 0.0 && shadowUV.y <= 1.0) {
		d = texture(u_cloudShadowMap, shadowUV.xy).r;
	} else {
		// Fallback for points outside the shadow map: evaluate noise directly
		d = evaluateCloudShadowDensityAtWorldPos(cloudPos.xz, time);
	}

	return mix(1.0, exp(-d), cloudShadowIntensity);
}

#ifdef USE_TERRAIN_DATA
// Forward declare terrain shadow coverage from terrain_shadows.glsl
float terrainShadowCoverage(vec3 worldPos, vec3 normal, vec3 lightDir);
#else
// Fallback if terrain data is not available
float terrainShadowCoverage(vec3 worldPos, vec3 normal, vec3 lightDir) {
	return 1.0;
}
#endif

/**
 * Calculate shadow factor for a fragment position using a specific shadow map.
 * Returns 0.0 if fully in shadow, 1.0 if fully lit.
 * Uses PCF (Percentage Closer Filtering) for soft shadow edges.
 */
float calculateShadow(int light_index, vec3 frag_pos, vec3 normal, vec3 light_dir) {
	// Optimization: Quick terrain raycast for directional lights (Sun)
	float terrainShadow = 1.0;
	if (lights[light_index].type == LIGHT_TYPE_DIRECTIONAL) {
		terrainShadow = terrainShadowCoverage(frag_pos, normal, light_dir);
		if (terrainShadow <= 0.0) {
			return terrainShadow;
		}
	}

	int shadow_index = lightShadowIndices[light_index];

	// Early out for invalid indices or when no shadow lights are active
	// This MUST return before any texture operations to avoid driver issues
	if (shadow_index < 0) {
		return terrainShadow; // No shadow for this light
	}
	if (numShadowLights <= 0) {
		return terrainShadow; // No shadow maps active at all
	}

	// Handle Cascaded Shadow Maps for directional lights
	int   cascade = 0;
	float cascade_blend = 0.0; // Blend factor for smooth cascade transitions
	int   next_cascade = -1;

	if (lights[light_index].type == LIGHT_TYPE_DIRECTIONAL) {
		// Use linear depth along camera forward for more consistent splits
		float depth = dot(frag_pos - viewPos, viewDir);
		cascade = -1;
		for (int i = 0; i < MAX_CASCADES; ++i) {
			if (depth < cascadeSplits[i]) {
				cascade = i;
				break;
			}
		}

		if (cascade == -1) {
			// Beyond all cascade splits - use the last cascade as catchall
			// The far cascade is configured with extended range specifically for this
			cascade = MAX_CASCADES - 1;
		}

		// Calculate blend zone for smooth cascade transitions
		// This eliminates the harsh "perspective shift" at cascade boundaries
		// Note: Don't blend for the last cascade since it's the catchall
		if (cascade < MAX_CASCADES - 1) {
			float cascade_start = (cascade == 0) ? 0.0 : cascadeSplits[cascade - 1];
			float cascade_end = cascadeSplits[cascade];
			float cascade_range = cascade_end - cascade_start;

			// Blend zone is the last 15% of each cascade
			float blend_zone_start = cascade_end - cascade_range * 0.15;
			if (depth > blend_zone_start) {
				cascade_blend = (depth - blend_zone_start) / (cascade_end - blend_zone_start);
				cascade_blend = smoothstep(0.0, 1.0, cascade_blend); // Smooth the transition
				next_cascade = cascade + 1;
			}
		}

		shadow_index += cascade;
	}

	if (shadow_index >= MAX_SHADOW_MAPS) {
		return terrainShadow; // Index out of bounds
	}

	// Transform fragment position to light space
	vec4 frag_pos_light_space = lightSpaceMatrices[shadow_index] * vec4(frag_pos, 1.0);

	// Perspective divide (guard against division by zero)
	if (abs(frag_pos_light_space.w) < 0.0001) {
		return terrainShadow;
	}
	vec3 proj_coords = frag_pos_light_space.xyz / frag_pos_light_space.w;

	// Transform to [0,1] range for texture sampling
	proj_coords = proj_coords * 0.5 + 0.5;

	// Check if fragment is outside the shadow map frustum
	if (proj_coords.x < 0.0 || proj_coords.x > 1.0 || proj_coords.y < 0.0 || proj_coords.y > 1.0 ||
	    proj_coords.z > 1.0 || proj_coords.z < 0.0) {
		return terrainShadow; // Outside shadow frustum, fully lit
	}

	// Current depth from light's perspective
	float current_depth = proj_coords.z;

	// Improved bias calculation to prevent shadow acne while keeping shadows connected to geometry
	// The key insight: larger cascades have lower resolution (larger texels), so need larger bias
	// But the bias should NOT be so large that shadows appear "floating" above terrain
	float slope_factor = max(1.0 - dot(normal, light_dir), 0.0); // 0 when facing light, 1 when perpendicular

	// Base bias: very small for direct facing surfaces
	float base_bias = 0.0001;

	// Slope bias: increases for steep angles relative to light
	float slope_bias = 0.001 * slope_factor;

	// Cascade-specific bias: accounts for texel size differences
	// CRITICAL: This should be modest - previous 5x multiplier was too aggressive
	// Near cascade (0): finest resolution, minimal extra bias needed
	// Far cascade (3): coarsest resolution, but still shouldn't be huge
	float cascade_bias_scale = 1.0 + float(cascade) * 0.8; // Was 5.0, now 0.8

	// Calculate texel size in world units for this cascade (approximate)
	vec2 texel_size = 1.0 / vec2(textureSize(shadowMaps, 0).xy);

	// Final bias combines all factors
	float bias = (base_bias + slope_bias) * cascade_bias_scale;

	// Clamp to prevent over-biasing that causes disconnected shadows
	bias = clamp(bias, 0.0001, 0.01);

	// PCF - sample multiple texels for soft shadows
	// Use larger kernel for distant cascades to match their lower resolution
	float shadow = 0.0;
	int   kernel_size = (cascade < 2) ? 1 : 2; // 3x3 for near, 5x5 for far
	float sample_count = 0.0;

	for (int x = -kernel_size; x <= kernel_size; ++x) {
		for (int y = -kernel_size; y <= kernel_size; ++y) {
			vec2 offset = vec2(x, y) * texel_size;
			// sampler2DArrayShadow expects (u, v, layer, compare_value)
			vec4 shadow_coord = vec4(proj_coords.xy + offset, float(shadow_index), current_depth - bias);
			shadow += texture(shadowMaps, shadow_coord);
			sample_count += 1.0;
		}
	}
	shadow /= sample_count;

	// Blend with next cascade if in transition zone
	// This smooths the visual transition and eliminates the "perspective shift" artifact
	if (cascade_blend > 0.0 && next_cascade >= 0 && next_cascade < MAX_CASCADES) {
		int next_shadow_index = shadow_index - cascade + next_cascade;
		if (next_shadow_index < MAX_SHADOW_MAPS) {
			// Sample from next cascade with its own bias
			float next_cascade_bias_scale = 1.0 + float(next_cascade) * 0.8;
			float next_bias = (base_bias + slope_bias) * next_cascade_bias_scale;
			next_bias = clamp(next_bias, 0.0001, 0.01);

			// Transform to next cascade's light space
			vec4 next_frag_pos_light_space = lightSpaceMatrices[next_shadow_index] * vec4(frag_pos, 1.0);
			if (abs(next_frag_pos_light_space.w) > 0.0001) {
				vec3 next_proj_coords = next_frag_pos_light_space.xyz / next_frag_pos_light_space.w;
				next_proj_coords = next_proj_coords * 0.5 + 0.5;

				// Only blend if also within next cascade's valid range
				if (next_proj_coords.x >= 0.0 && next_proj_coords.x <= 1.0 && next_proj_coords.y >= 0.0 &&
				    next_proj_coords.y <= 1.0 && next_proj_coords.z >= 0.0 && next_proj_coords.z <= 1.0) {
					float next_shadow = 0.0;
					int   next_kernel_size = (next_cascade < 2) ? 1 : 2;
					float next_sample_count = 0.0;

					for (int x = -next_kernel_size; x <= next_kernel_size; ++x) {
						for (int y = -next_kernel_size; y <= next_kernel_size; ++y) {
							vec2 offset = vec2(x, y) * texel_size;
							vec4 shadow_coord = vec4(
								next_proj_coords.xy + offset,
								float(next_shadow_index),
								next_proj_coords.z - next_bias
							);
							next_shadow += texture(shadowMaps, shadow_coord);
							next_sample_count += 1.0;
						}
					}
					next_shadow /= next_sample_count;

					// Blend between cascades
					shadow = mix(shadow, next_shadow, cascade_blend);
				}
			}
		}
	}

	return min(terrainShadow, shadow);
}

/**
 * Look up and interpolate octahedral probe ambient irradiance for a fragment.
 */
vec3 getSpatialAmbientProbe(vec3 worldPos, vec3 N) {
	vec3  p_origin = u_probeParams.xyz;
	vec2  p_size_xz = u_probeParams.zw;
	float p_size_y = u_probeParams2.x;
	float p_spacing = u_probeParams2.y;
	float p_oct_res = u_probeParams2.z;

	// Convert world position to grid space
	// Grid mapping: x and z are horizontal, y is vertical.
	// worldPos.y maps to gridCoord.z
	vec3  gridPos;
	gridPos.x = (worldPos.x - p_origin.x) / p_spacing;
	gridPos.y = (worldPos.z - p_origin.y) / p_spacing;
	gridPos.z = (worldPos.y) / p_spacing; // Vertical origin is 0

	vec3  base = floor(gridPos);
	vec3  f = fract(gridPos);

	vec2  octUV = octEncode(N);
	vec3  accumIrradiance = vec3(0.0);

	for (int i = 0; i < 8; ++i) {
		ivec3 offset = ivec3(i & 1, (i >> 1) & 1, (i >> 2) & 1);
		ivec3 coord = ivec3(base) + offset;

		if (coord.x >= 0 && coord.x < int(p_size_xz.x) &&
		    coord.y >= 0 && coord.y < int(p_size_xz.y) &&
		    coord.z >= 0 && coord.z < int(p_size_y)) {

			// Map grid coord + octahedral UV to 3D texture space
			// Texture size: (size_x * oct_res, size_z * oct_res, size_y)
			vec3 texCoord;
			texCoord.x = (float(coord.x) + octUV.x) / p_size_xz.x;
			texCoord.y = (float(coord.y) + octUV.y) / p_size_xz.y;
			texCoord.z = (float(coord.z) + 0.5) / p_size_y;

			vec3 sampleIrradiance = textureLod(u_probeIrradiance, texCoord, 0.0).rgb;

			float weight = (offset.x > 0 ? f.x : 1.0 - f.x) *
			               (offset.y > 0 ? f.y : 1.0 - f.y) *
			               (offset.z > 0 ? f.z : 1.0 - f.z);

			accumIrradiance += sampleIrradiance * weight;
		}
	}

	return max(accumIrradiance, 0.0);
}

/**
 * Evaluate Spherical Harmonics irradiance for a given normal and set of coefficients.
 */
vec3 evalSHIrradianceFromCoeffs(vec3 n, vec4 coeffs[9]) {
	float c1 = 0.282095;
	float c2 = 0.488603;
	float c3 = 1.092548;
	float c4 = 0.315392;
	float c5 = 0.546274;

	float a0 = 3.141593;
	float a1 = 2.094395;
	float a2 = 0.785398;

	vec3 res = vec3(0.0);
	res += a0 * c1 * coeffs[0].rgb;
	res += a1 * c2 * (coeffs[1].rgb * n.y + coeffs[2].rgb * n.z + coeffs[3].rgb * n.x);
	res += a2 * c3 * (coeffs[4].rgb * n.x * n.y + coeffs[5].rgb * n.y * n.z + coeffs[7].rgb * n.x * n.z);
	res += a2 * c4 * coeffs[6].rgb * (3.0 * n.y * n.y - 1.0);
	res += a2 * c5 * coeffs[8].rgb * (n.x * n.x - n.z * n.z);
	return max(res, 0.0);
}

/**
 * Evaluate Spherical Harmonics irradiance for a given normal.
 * Uses 2nd-order SH coefficients from the Lighting UBO.
 */
vec3 evalSHIrradiance(vec3 n) {
	return evalSHIrradianceFromCoeffs(n, sh_coeffs);
}

#ifdef USE_TERRAIN_DATA
vec3 getSpatialAmbientSH(vec3 worldPos, vec3 N) {
	vec3 probeAmbient = getSpatialAmbientProbe(worldPos, N);
	vec3 fallbackAmbient = evalSHIrradiance(N);

	// Use probe if available and not purely black (probes initialized to black/zero)
	return length(probeAmbient) > 0.0 ? probeAmbient : fallbackAmbient;
}
// Forward declare macro occlusion from terrain_shadows.glsl
float calculateTerrainOcclusion(vec3 worldPos, vec3 normal);
#else
vec3 getSpatialAmbientSH(vec3 worldPos, vec3 N) {
	return vec3(0.1); // Fallback
}
float calculateTerrainOcclusion(vec3 worldPos, vec3 normal) {
	return 1.0;
}
#endif

/**
 * Calculate the relative luminance of a color.
 * Used for determining how much a specular highlight should contribute to fragment opacity.
 */
float get_luminance(vec3 color) {
	return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

// ============================================================================
// PBR Lighting Functions (Cook-Torrance BRDF)
// ============================================================================

// PBR intensity multiplier to compensate for energy conservation
// PBR is inherently darker than legacy Phong.
const float PBR_INTENSITY_BOOST = 1.0;

void evaluate_brdf(
    vec3 N, vec3 V, vec3 L, vec3 albedo, float roughness, float metallic, vec3 F0,
    vec3 radiance, float shadow, inout vec3 Lo, inout float spec_lum)
{
    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0) return; // Early out

    float NdotV = max(dot(N, V), 1e-4);
    vec3 H = normalize(V + L);
    float HdotV = max(dot(H, V), 0.0);

    float NDF = DistributionGGX(N, H, roughness);
    float V_term = VisibilitySmithGGXCorrelated(NdotL, NdotV, roughness);
    vec3 F = fresnelSchlickFast(HdotV, F0);
    vec3 specular = NDF * V_term * F;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    vec3 specular_radiance = specular * radiance * NdotL * shadow;

    Lo += (kD * albedo / PI) * radiance * NdotL * shadow + specular_radiance;
    spec_lum += get_luminance(specular_radiance);
}

/**
 * PBR lighting with Cook-Torrance BRDF - supports all light types.
 * Returns vec4(color.rgb, specular_luminance).
 *
 * @param frag_pos Fragment world position
 * @param normal Surface normal (must be normalized)
 * @param albedo Base color of the material
 * @param roughness Surface roughness [0=smooth, 1=rough]
 * @param metallic Metallic property [0=dielectric, 1=metal]
 * @param ao Ambient occlusion [0=fully occluded, 1=no occlusion]
 */
vec4 apply_lighting_pbr(vec3 frag_pos, vec3 normal, vec3 albedo, float roughness, float metallic, float ao, out float primaryShadow) {
	primaryShadow = 1.0;
	vec3 N = normalize(normal);
	vec3 V = normalize(viewPos - frag_pos);

	// Calculate reflectance at normal incidence
	// For dielectrics use 0.04, for metals use albedo color
	vec3 F0 = vec3(0.04);
	F0 = mix(F0, albedo, metallic);

	vec3  Lo = vec3(0.0);
	float spec_lum = 0.0;

	// ------------------------------------------------------------------
	// PASS 1: Global Directional Light (Sun/Moon)
	// ------------------------------------------------------------------
	for (int i = 0; i < min(2, num_lights); ++i) {
		if (lights[i].type != LIGHT_TYPE_DIRECTIONAL) {
			continue;
		}

		if (lights[i].intensity <= 0.0) {
			continue;
		}

		vec3 L;
		float base_attenuation; // Unused for directional, but needed for your function signature
		calculateLightContribution(i, frag_pos, L, base_attenuation);

		// Horizon check and normal-facing check
		if (L.y <= 0.0 || dot(N, L) <= 0.0) continue;

		// Atmospheric transmittance isolated outside the loop
		float r = kEarthRadiusKM + (frag_pos.y / (1000.0 * worldScale));
		vec3 atmosphereTransmittance = texture(u_transmittanceLUT, getTransmittanceUV(r, L.y)).rgb;

		float attenuation = lights[i].intensity * PBR_INTENSITY_BOOST;
		vec3 radiance = lights[i].color * attenuation * atmosphereTransmittance;

		// Cloud shadows isolated outside the loop
		float shadow = calculateShadow(i, frag_pos, N, L);
		shadow *= calculateCloudShadow(i, frag_pos);

		primaryShadow = min(primaryShadow, shadow);

		evaluate_brdf(N, V, L, albedo, roughness, metallic, F0, radiance, shadow, Lo, spec_lum);
	}

	// ------------------------------------------------------------------
	// PASS 2: Local Lights (Point/Spot)
	// ------------------------------------------------------------------
	for (int i = 2; i < num_lights; ++i) {
		if (lights[i].intensity <= 0.0) {
			continue;
		}

		vec3 L;
		float base_attenuation;
		calculateLightContribution(i, frag_pos, L, base_attenuation);

		// Quick culling before calculating attenuation or shadows
		if (dot(N, L) <= 0.0) continue;

		float attenuation = (lights[i].intensity * PBR_INTENSITY_BOOST) * base_attenuation;
		vec3 radiance = lights[i].color * attenuation;

		float shadow = calculateShadow(i, frag_pos, N, L);

		evaluate_brdf(N, V, L, albedo, roughness, metallic, F0, radiance, shadow, Lo, spec_lum);
	}

	// Spatially-varying SH ambient augmented with macro occlusion
	float terrainOcc = calculateTerrainOcclusion(frag_pos, N);
	vec3  spatialSHAmbient = getSpatialAmbientSH(frag_pos, N);

	float combinedAO = ao * terrainOcc;
	vec3  ambientDiffuse = spatialSHAmbient * albedo * combinedAO;

	// Scale down ambient overall to maintain shadow contrast and prevent "flat" look
	// ambientDiffuse *= 0.75;

	// Environment reflection approximation for glossy surfaces
	vec3 R = reflect(-V, N);

	// Fresnel at grazing angles - smooth surfaces reflect more at edges
	vec3  F0_env = mix(vec3(0.04), albedo, metallic);
	float NdotV = max(dot(N, V), 0.0);
	vec3  F_env = fresnelSchlickRoughness(NdotV, F0_env, roughness);

	// Fake environment color using spatial SH
	vec3 envColor = getSpatialAmbientSH(frag_pos, R);

	// Attenuate reflection by occlusion to prevent glow in caves/valleys
	envColor *= terrainOcc;

	// Environment reflection strength based on smoothness
	float smoothness = 1.0 - roughness;
	float envStrength = smoothness * smoothness * 0.8;

	// Metallic surfaces should reflect the environment color tinted by albedo
	// Non-metallic surfaces reflect environment but less strongly
	vec3 ambientSpecular = F_env * envColor * envStrength * combinedAO;

	// Combine diffuse and specular ambient
	vec3 ambient = ambientDiffuse * (1.0 - metallic * 0.9) + ambientSpecular;
	vec3 color = ambient + Lo;

	return vec4(color, spec_lum + get_luminance(ambientSpecular));
}

void evaluate_foliage_brdf(
    vec3 N, vec3 V, vec3 L, vec3 albedo, float roughness, float metallic, vec3 F0,
    vec3 radiance, float shadow, float translucency, inout vec3 Lo, inout float spec_lum)
{
    float NdotL = dot(N, L);
    float NdotV = max(dot(N, V), 1e-4);

    // 1. FRONT-SIDE REFLECTION (Standard PBR)
    // Only calculate specular if the light is actually in front of the normal
    if (NdotL > 0.0) {
        vec3 H = normalize(V + L);
        float HdotV = max(dot(H, V), 0.0);

        float NDF = DistributionGGX(N, H, roughness);
        float V_term = VisibilitySmithGGXCorrelated(NdotL, NdotV, roughness);
        vec3  F = fresnelSchlickFast(HdotV, F0);

        vec3 specular = NDF * V_term * F;
        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

        vec3 specular_out = specular * radiance * NdotL * shadow;
        Lo += (kD * albedo / PI) * radiance * NdotL * shadow + specular_out;
        spec_lum += get_luminance(specular_out);
    }

    // 2. BACK-SIDE TRANSMISSION (Subsurface Scattering approximation)
    // If the light is behind the camera-facing normal, it "bleeds" through.
    // We use a simplified 'Wrapped' or 'Inverted' diffuse model.
    float transmissionNdotL = max(-NdotL, 0.0);
    if (transmissionNdotL > 0.0) {
        // Translucent light is generally filtered by the material color (albedo)
        // and doesn't have a specular component.
        vec3 transmission_out = (albedo / PI) * radiance * transmissionNdotL * translucency * shadow;
        Lo += transmission_out;
    }
}

vec4 apply_lighting_foliage(vec3 frag_pos, vec3 normal, vec3 albedo, float roughness, float metallic, float ao, out float primaryShadow) {
    primaryShadow = 1.0;
    vec3 N = normalize(normal); // Already flipped via gl_FrontFacing in grass.frag
    vec3 V = normalize(viewPos - frag_pos);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Lo = vec3(0.0);
    float spec_lum = 0.0;

    // How much light bleeds through the grass (0.0 to 1.0)
    float translucency = 0.5;

    // PASS 1: Directional Light (with Atmosphere & Clouds)
	for (int i = 0; i < min(2, num_lights); ++i) {
		if (lights[i].type != LIGHT_TYPE_DIRECTIONAL) {
			continue;
		}
		if (lights[i].intensity <= 0.0) {
			continue;
		}

        vec3 L; float atten;
        calculateLightContribution(i, frag_pos, L, atten);

		// Horizon check
		if (L.y <= 0.0) continue;

        float r = kEarthRadiusKM + (frag_pos.y / (1000.0 * worldScale));
        vec3 atmosphereTransmittance = texture(u_transmittanceLUT, getTransmittanceUV(r, L.y)).rgb;
        vec3 radiance = lights[i].color * (lights[i].intensity * PBR_INTENSITY_BOOST) * atmosphereTransmittance;

        float shadow = min(calculateShadow(i, frag_pos, N, L), calculateCloudShadow(i, frag_pos));

		primaryShadow = min(primaryShadow, shadow);

        evaluate_foliage_brdf(N, V, L, albedo, roughness, metallic, F0, radiance, shadow, translucency, Lo, spec_lum);
    }

    // PASS 2: Local Lights
    for (int i = min(2, num_lights); i < num_lights; ++i) {
		if (lights[i].intensity <= 0.0) {
			continue;
		}

        vec3 L; float base_atten;
        calculateLightContribution(i, frag_pos, L, base_atten);

        // We only skip if the light is perpendicular (rare)
        if (abs(dot(N, L)) < 0.0001) continue;

        vec3 radiance = lights[i].color * (lights[i].intensity * PBR_INTENSITY_BOOST) * base_atten;
        float shadow = calculateShadow(i, frag_pos, N, L);

        evaluate_foliage_brdf(N, V, L, albedo, roughness, metallic, F0, radiance, shadow, translucency, Lo, spec_lum);
    }

    // Standard Ambient (using your existing SH logic)
    float terrainOcc = calculateTerrainOcclusion(frag_pos, N);
    vec3 ambient = (getSpatialAmbientSH(frag_pos, N) * albedo) * (ao * terrainOcc);

    return vec4(ambient + Lo, spec_lum);
}

/**
 * PBR lighting without shadows - for shaders that don't need shadow calculations.
 * Supports all light types (point, directional, spot).
 * Returns vec4(color.rgb, specular_luminance).
 */
vec4 apply_lighting_pbr_no_shadows(vec3 frag_pos, vec3 normal, vec3 albedo, float roughness, float metallic, float ao, out float primaryShadow) {
	primaryShadow = 1.0;
	vec3 N = normalize(normal);
	vec3 V = normalize(viewPos - frag_pos);

	vec3 F0 = vec3(0.04);
	F0 = mix(F0, albedo, metallic);

	vec3  Lo = vec3(0.0);
	float spec_lum = 0.0;

	for (int i = 0; i < num_lights; ++i) {
		vec3  L;
		float base_attenuation;
		calculateLightContribution(i, frag_pos, L, base_attenuation);

		vec3 H = normalize(V + L);

		float attenuation;
		vec3  atmosphereTransmittance = vec3(1.0);

		if (lights[i].type == LIGHT_TYPE_DIRECTIONAL) {
			attenuation = lights[i].intensity * PBR_INTENSITY_BOOST;

			// Apply atmospheric attenuation
			float r = kEarthRadiusKM + (frag_pos.y / (1000.0 * worldScale));
			float mu = L.y;
			atmosphereTransmittance = texture(u_transmittanceLUT, getTransmittanceUV(r, mu)).rgb;
		} else {
			attenuation = (lights[i].intensity * PBR_INTENSITY_BOOST) * base_attenuation;
		}
		vec3 radiance = lights[i].color * attenuation * atmosphereTransmittance;

		float NDF = DistributionGGX(N, H, roughness);
		float G = GeometrySmith(N, V, L, roughness);
		vec3  F = fresnelSchlick(max(dot(H, V), 0.0), F0);

		vec3  numerator = NDF * G * F;
		float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
		vec3  specular = numerator / denominator;

		vec3 kS = F;
		vec3 kD = vec3(1.0) - kS;
		kD *= 1.0 - metallic;

		float NdotL = max(dot(N, L), 0.0);

		// Apply cloud shadow for directional lights
		float shadow = 1.0;
		if (lights[i].type == LIGHT_TYPE_DIRECTIONAL) {
			shadow *= calculateCloudShadow(i, frag_pos);
		}

		if (i == 0)
			primaryShadow = shadow;

		vec3 specular_radiance = specular * radiance * NdotL * shadow;
		Lo += (kD * albedo / PI) * radiance * NdotL * shadow + specular_radiance;
		spec_lum += get_luminance(specular_radiance);
	}

	// Spatially-varying SH ambient augmented with macro occlusion
	float terrainOcc = calculateTerrainOcclusion(frag_pos, N);
	vec3  spatialSHAmbient = getSpatialAmbientSH(frag_pos, N);

	float combinedAO = ao * terrainOcc;
	vec3  ambientDiffuse = spatialSHAmbient * albedo * combinedAO;

	// Scale down ambient overall to maintain shadow contrast
	ambientDiffuse *= 0.75;

	vec3 R = reflect(-V, N);

	vec3  F0_env = mix(vec3(0.04), albedo, metallic);
	float NdotV = max(dot(N, V), 0.0);
	vec3  F_env = fresnelSchlickRoughness(NdotV, F0_env, roughness);

	// Fake environment color using spatial SH
	vec3 envColor = getSpatialAmbientSH(frag_pos, R);

	// Attenuate reflection by occlusion to prevent glow in caves/valleys
	envColor *= terrainOcc;

	float smoothness = 1.0 - roughness;
	float envStrength = smoothness * smoothness * 0.8;
	vec3  ambientSpecular = F_env * envColor * envStrength * combinedAO;
	vec3  ambient = ambientDiffuse * (1.0 - metallic * 0.9) + ambientSpecular;

	return vec4(ambient + Lo, spec_lum + get_luminance(ambientSpecular));
}

// ============================================================================
// Legacy/Phong Lighting Functions
// ============================================================================

/**
 * Apply lighting with shadow support - supports all light types.
 * Returns vec4(color.rgb, specular_luminance).
 */
vec4 apply_lighting(vec3 frag_pos, vec3 normal, vec3 albedo, float specular_strength, out float primaryShadow) {
	primaryShadow = 1.0;
	vec3  result = ambient_light * albedo;
	float spec_lum = 0.0;

	for (int i = 0; i < num_lights; ++i) {
		vec3  light_dir;
		float attenuation;
		calculateLightContribution(i, frag_pos, light_dir, attenuation);

		// Calculate shadow factor for this light with slope-scaled bias
		float shadow = calculateShadow(i, frag_pos, normal, light_dir);

		// Atmospheric attenuation for directional light
		vec3 atmosphereTransmittance = vec3(1.0);
		if (lights[i].type == LIGHT_TYPE_DIRECTIONAL) {
			float r = kEarthRadiusKM + (frag_pos.y / (1000.0 * worldScale));
			float mu = light_dir.y;
			atmosphereTransmittance = texture(u_transmittanceLUT, getTransmittanceUV(r, mu)).rgb;

			// Apply cloud shadow
			shadow *= calculateCloudShadow(i, frag_pos);
		}

		if (i == 0)
			primaryShadow = shadow;

		// Diffuse
		float diff = max(dot(normal, light_dir), 0.0);
		vec3  diffuse = lights[i].color * atmosphereTransmittance * diff * albedo;

		// Specular (Blinn-Phong)
		vec3  view_dir = normalize(viewPos - frag_pos);
		vec3  reflect_dir = reflect(-light_dir, normal);
		float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32);
		vec3  specular_contribution = lights[i].color * atmosphereTransmittance * spec * specular_strength *
			lights[i].intensity * shadow * attenuation;

		// Apply shadow and attenuation to diffuse and specular, but not ambient
		result += (diffuse * lights[i].intensity * shadow * attenuation) + specular_contribution;
		spec_lum += get_luminance(specular_contribution);
	}

	return vec4(result, spec_lum);
}

/**
 * Apply lighting without shadows - supports all light types.
 * Returns vec4(color.rgb, specular_luminance).
 */
vec4 apply_lighting_no_shadows(vec3 frag_pos, vec3 normal, vec3 albedo, float specular_strength, out float primaryShadow) {
	primaryShadow = 1.0;
	vec3  result = ambient_light * albedo;
	float spec_lum = 0.0;

	for (int i = 0; i < num_lights; ++i) {
		vec3  light_dir;
		float attenuation;
		calculateLightContribution(i, frag_pos, light_dir, attenuation);

		// Atmospheric attenuation for directional light
		vec3  atmosphereTransmittance = vec3(1.0);
		if (lights[i].type == LIGHT_TYPE_DIRECTIONAL) {
			float r = kEarthRadiusKM + (frag_pos.y / (1000.0 * worldScale));
			float mu = light_dir.y;
			atmosphereTransmittance = texture(u_transmittanceLUT, getTransmittanceUV(r, mu)).rgb;
		}

		// Diffuse
		float diff = max(dot(normal, light_dir), 0.0);
		vec3  diffuse = lights[i].color * atmosphereTransmittance * diff * albedo;

		// Specular
		vec3  view_dir = normalize(viewPos - frag_pos);
		vec3  reflect_dir = reflect(-light_dir, normal);
		float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32);
		vec3  specular_contribution = lights[i].color * atmosphereTransmittance * spec * specular_strength *
			lights[i].intensity * attenuation;

		result += (diffuse * lights[i].intensity * attenuation) + specular_contribution;
		spec_lum += get_luminance(specular_contribution);
	}

	return vec4(result, spec_lum);
}

// ============================================================================
// Iridescent/Special Effect Lighting
// ============================================================================

/**
 * Calculate iridescent color based on view angle and surface normal.
 * Returns a rainbow-shifted color that changes with viewing angle.
 *
 * @param view_dir Normalized view direction
 * @param normal Normalized surface normal
 * @param base_color Optional base color to blend with
 * @param time_offset Animation time for swirling effect
 */
vec3 calculate_iridescence(vec3 view_dir, vec3 normal, vec3 base_color, float time_offset) {
	// Fresnel-based angle factor
	float NdotV = abs(dot(view_dir, normal));
	float angle_factor = 1.0 - NdotV;
	angle_factor = pow(angle_factor, 2.0);

	// Add time-based swirl for animation
	float swirl = sin(time_offset * 0.5) * 0.5 + 0.5;
	vec3  iridescent = vec3(
		sin(angle_factor * 10.0 + swirl * 5.0) * 0.5 + 0.5,
		sin(angle_factor * 10.0 + swirl * 5.0 + 2.0) * 0.5 + 0.5,
		sin(angle_factor * 10.0 + swirl * 5.0 + 4.0) * 0.5 + 0.5
	);

	// Blend with base color based on angle
	return mix(base_color, iridescent, angle_factor * 0.8 + 0.2);
}

/**
 * PBR iridescent material - combines PBR lighting with thin-film interference.
 * Returns vec4(color.rgb, specular_luminance).
 *
 * @param frag_pos Fragment world position
 * @param normal Surface normal
 * @param base_color Base/underlying color
 * @param roughness Surface roughness [0=mirror, 1=matte]
 * @param iridescence_strength How much iridescence to apply [0-1]
 */
vec4 apply_lighting_pbr_iridescent_no_shadows(
	vec3  frag_pos,
	vec3  normal,
	vec3  base_color,
	float roughness,
	float iridescence_strength,
	out float primaryShadow
) {
	primaryShadow = 1.0;
	vec3  N = normalize(normal);
	vec3  V = normalize(viewPos - frag_pos);
	float NdotV = max(dot(N, V), 0.0);

	// Calculate iridescent color based on view angle
	vec3 iridescent_color = calculate_iridescence(V, N, base_color, time + frag_pos.y * 2.0);

	// Base iridescent appearance (always visible, angle-dependent)
	float angle_factor = 1.0 - NdotV;
	vec3  base_iridescent = iridescent_color * (0.4 + angle_factor * 0.6);

	// Add specular highlights from lights
	vec3  specular_total = vec3(0.0);
	float spec_lum = 0.0;

	for (int i = 0; i < num_lights; ++i) {
		vec3  L;
		float base_attenuation;
		calculateLightContribution(i, frag_pos, L, base_attenuation);

		vec3  H = normalize(V + L);
		float NdotL = max(dot(N, L), 0.0);
		float HdotV = max(dot(H, V), 0.0);

		float attenuation;
		vec3  atmosphereTransmittance = vec3(1.0);

		if (lights[i].type == LIGHT_TYPE_DIRECTIONAL) {
			attenuation = lights[i].intensity * PBR_INTENSITY_BOOST;

			// Apply atmospheric attenuation
			float r = kEarthRadiusKM + (frag_pos.y / (1000.0 * worldScale));
			float mu = L.y;
			atmosphereTransmittance = texture(u_transmittanceLUT, getTransmittanceUV(r, mu)).rgb;
		} else {
			attenuation = (lights[i].intensity * PBR_INTENSITY_BOOST) * base_attenuation;
		}
		vec3 radiance = lights[i].color * attenuation * atmosphereTransmittance;

		// GGX specular for sharp highlights
		float NDF = DistributionGGX(N, H, roughness);
		float G = GeometrySmith(N, V, L, roughness);

		// Fresnel with iridescent F0
		vec3 F0 = iridescent_color * 0.8 + vec3(0.2);
		vec3 F = fresnelSchlick(HdotV, F0);

		vec3  numerator = NDF * G * F;
		float denominator = 4.0 * NdotV * NdotL + 0.0001;
		vec3  specular = numerator / denominator;

		vec3 specular_contribution = specular * radiance * NdotL;
		specular_total += specular_contribution;
		spec_lum += get_luminance(specular_contribution);
	}

	// Fresnel rim - strong white/iridescent edge glow
	float fresnel_rim = pow(1.0 - NdotV, 4.0);
	vec3  rim_color = mix(vec3(1.0), iridescent_color, 0.5) * fresnel_rim * 0.8;

	// Combine: base iridescent appearance + specular highlights + rim
	return vec4(base_iridescent + specular_total + rim_color, spec_lum + get_luminance(rim_color));
}

// ============================================================================
// Emissive and Flash Effect Functions
// ============================================================================

/**
 * Emissive glow calculation for rocket/flame trails.
 * Creates a hot, glowing effect that emits light.
 *
 * @param base_emission Base emissive color (e.g., orange for flames)
 * @param intensity Glow intensity multiplier
 * @param falloff How quickly the glow fades [0=sharp, 1=soft]
 */
vec3 calculate_emission(vec3 base_emission, float intensity, float falloff) {
	// HDR emission - can exceed 1.0 for bloom effects
	return base_emission * intensity * (1.0 + falloff);
}

/**
 * Render a glowing/emissive object surface.
 * Combines emissive self-illumination with optional environmental lighting.
 * Use this for objects that ARE the light source (lamps, magic orbs, etc.)
 * Returns vec4(color.rgb, specular_luminance).
 *
 * @param frag_pos Fragment world position
 * @param normal Surface normal
 * @param emissive_color The glow color of the object
 * @param emissive_intensity How bright the glow is (can exceed 1.0 for HDR/bloom)
 * @param base_albedo Optional base color for non-emissive parts
 * @param emissive_coverage How much of surface is emissive [0=none, 1=fully glowing]
 */
vec4 apply_emissive_surface(
	vec3  frag_pos,
	vec3  normal,
	vec3  emissive_color,
	float emissive_intensity,
	vec3  base_albedo,
	float emissive_coverage,
	out float primaryShadow
) {
	primaryShadow = 1.0;
	vec3 N = normalize(normal);
	vec3 V = normalize(viewPos - frag_pos);

	// The emissive part - self-illuminated, not affected by external lighting
	vec3 emission = emissive_color * emissive_intensity;

	// Add subtle Fresnel glow at edges for a more volumetric feel
	float fresnel = pow(1.0 - max(dot(N, V), 0.0), 3.0);
	emission += emissive_color * fresnel * emissive_intensity * 0.5;

	// The non-emissive part gets regular lighting
	vec4 lit_surface = vec4(0.0);
	if (emissive_coverage < 1.0) {
		lit_surface = apply_lighting_no_shadows(frag_pos, normal, base_albedo, 0.5, primaryShadow);
	}

	// Blend between emissive and lit surface
	// Emissions are considered fully opaque specular-like contributors for alpha purposes
	return mix(lit_surface, vec4(emission, get_luminance(emission)), emissive_coverage);
}

/**
 * Render a glowing object with PBR properties for non-emissive regions.
 * The emissive parts glow while other parts use PBR lighting.
 * Returns vec4(color.rgb, specular_luminance).
 *
 * @param frag_pos Fragment world position
 * @param normal Surface normal
 * @param emissive_color The glow color of the object
 * @param emissive_intensity Glow brightness (box HDR, can exceed 1.0)
 * @param base_albedo Base color for non-emissive parts
 * @param roughness PBR roughness for non-emissive parts
 * @param metallic PBR metallic for non-emissive parts
 * @param emissive_mask Per-fragment emissive coverage [0-1]
 */
vec4 apply_emissive_surface_pbr(
	vec3  frag_pos,
	vec3  normal,
	vec3  emissive_color,
	float emissive_intensity,
	vec3  base_albedo,
	float roughness,
	float metallic,
	float emissive_mask,
	out float primaryShadow
) {
	primaryShadow = 1.0;
	vec3 N = normalize(normal);
	vec3 V = normalize(viewPos - frag_pos);

	// Emissive component
	vec3  emission = emissive_color * emissive_intensity;
	float fresnel = pow(1.0 - max(dot(N, V), 0.0), 3.0);
	emission += emissive_color * fresnel * emissive_intensity * 0.3;

	// PBR lit component for non-emissive parts
	vec4 pbr_lit = apply_lighting_pbr_no_shadows(frag_pos, normal, base_albedo, roughness, metallic, 1.0, primaryShadow);

	// Blend based on emissive mask
	return mix(pbr_lit, vec4(emission, get_luminance(emission)), emissive_mask);
}

/**
 * Calculate flash/explosion illumination contribution to a surface.
 * Call this in addition to regular lighting for surfaces hit by a flash.
 * Returns additive light contribution.
 *
 * @param frag_pos Fragment world position
 * @param normal Surface normal
 * @param flash_pos Explosion/flash center position
 * @param flash_color Flash color (typically warm white/orange)
 * @param flash_intensity Flash brightness (can be very high, e.g., 10-50)
 * @param flash_radius Effective radius of the flash
 * @param flash_time Normalized time since flash [0=peak, 1=faded]
 */
vec3 calculate_flash_contribution(
	vec3  frag_pos,
	vec3  normal,
	vec3  flash_pos,
	vec3  flash_color,
	float flash_intensity,
	float flash_radius,
	float flash_time
) {
	vec3  L = normalize(flash_pos - frag_pos);
	float distance = length(flash_pos - frag_pos);
	float NdotL = max(dot(normalize(normal), L), 0.0);

	// Distance attenuation with sharp falloff
	float norm_dist = distance / max(flash_radius, 0.001);
	float dist_atten = 1.0 / pow(1.0 + norm_dist, 2.0);
	dist_atten *= smoothstep(2.0, 1.0, norm_dist); // Hard cutoff

	// Time-based fade (flash decays rapidly)
	// Starts bright, fades quickly, with slight persistence
	float time_atten = pow(1.0 - clamp(flash_time, 0.0, 1.0), 3.0);

	// Combine for final flash contribution
	return flash_color * flash_intensity * dist_atten * time_atten * NdotL;
}

/**
 * Full flash effect - returns both the flash illumination and suggested bloom.
 * Use the bloom value to drive post-processing bloom intensity.
 *
 * @param frag_pos Fragment world position
 * @param normal Surface normal
 * @param albedo Surface base color
 * @param flash_pos Flash center
 * @param flash_color Flash color
 * @param flash_intensity Flash brightness
 * @param flash_radius Flash radius
 * @param flash_time Normalized time [0=peak, 1=faded]
 * @param out_bloom Output: suggested bloom intensity for post-processing
 */
vec3 apply_flash_lighting(
	vec3      frag_pos,
	vec3      normal,
	vec3      albedo,
	vec3      flash_pos,
	vec3      flash_color,
	float     flash_intensity,
	float     flash_radius,
	float     flash_time,
	out float out_bloom
) {
	vec3 flash = calculate_flash_contribution(
		frag_pos,
		normal,
		flash_pos,
		flash_color,
		flash_intensity,
		flash_radius,
		flash_time
	);

	// Flash contribution adds to surface color
	vec3 result = albedo * flash;

	// Calculate bloom intensity based on flash brightness at this point
	// Surfaces closer to flash should bloom more
	out_bloom = length(flash) * 0.1; // Scale for bloom post-process

	return result;
}

#endif // HELPERS_LIGHTING_GLSL
#endif // GSHADERS_HELPERS_LIGHTING_GLSL
