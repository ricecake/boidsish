#version 430 core
#ifndef GSHADERS_TERRAIN_FRAG
#define GSHADERS_TERRAIN_FRAG
layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec2 Velocity;

in vec3       Normal;
in vec3       FragPos;
in vec4       CurPosition;
in vec4       PrevPosition;
in vec2       TexCoords;
flat in float TextureSlice;
in float      perturbFactor;
in float      tessFactor;
in float      vIsWater;
in float      vErosionDelta;
in float      vRidgeMap;
in float      vSubstrate;

//START shaders/helpers/erosion.glsl
#ifndef GSHADERS_HELPERS_EROSION_GLSL
#define GSHADERS_HELPERS_EROSION_GLSL
#ifndef HELPERS_EROSION_GLSL
#define HELPERS_EROSION_GLSL

/*
================================================================================
Advanced terrain erosion filter based on stacked faded gullies.
Adapted from Rune Skovbo Johansen's technique (runevision).
Original source: https://www.shadertoy.com/view/33cXW8
Blog post: https://blog.runevision.com/2026/03/fast-and-gorgeous-erosion-filter.html

Phacelle Noise function copyright (c) 2025 Rune Skovbo Johansen
Advanced Terrain Erosion Filter copyright (c) 2025 Rune Skovbo Johansen

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
================================================================================
*/

#define TAU 6.28318530717959

// Compatible hash function returning vec2 in range [-1, 1]
vec2 hash(vec2 p) {
	p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
	return fract(sin(p) * 43758.5453123) * 2.0 - 1.0;
}

float clamp01(float t) {
	return clamp(t, 0.0, 1.0);
}

// The Simple Phacelle Noise function produces a stripe pattern aligned with the input vector.
// The name Phacelle is a portmanteau of phase and cell, since the function produces a phase by
// interpolating cosine and sine waves from multiple cells.
//  - p is the input point being evaluated.
//  - normDir is the direction of the stripes at this point. It must be a normalized vector.
//  - freq is the freqency of the stripes within each cell. It's best to keep it close to 1.0.
//  - offset is the phase offset of the stripes, where 1.0 is a full cycle.
//  - normalization is the degree of normalization applied, between 0 and 1.
vec4 PhacelleNoise(in vec2 p, vec2 normDir, float freq, float offset, float normalization) {
	// Get a vector orthogonal to the input direction, with a
	// magnitude proportional to the frequency of the stripes.
	vec2 sideDir = normDir.yx * vec2(-1.0, 1.0) * freq * TAU;
	offset *= TAU;

	vec2  pInt = floor(p);
	vec2  pFrac = fract(p);
	vec2  phaseDir = vec2(0.0);
	float weightSum = 0.0;
	for (int i = -1; i <= 2; i++) {
		for (int j = -1; j <= 2; j++) {
			vec2 gridOffset = vec2(i, j);
			vec2 gridPoint = pInt + gridOffset;
			vec2 randomOffset = hash(gridPoint) * 0.5;
			vec2 vectorFromCellPoint = pFrac - gridOffset - randomOffset;

			// Bell-shaped weight function
			float sqrDist = dot(vectorFromCellPoint, vectorFromCellPoint);
			float weight = exp(-sqrDist * 2.0);
			weight = max(0.0, weight - 0.01111);

			weightSum += weight;
			float waveInput = dot(vectorFromCellPoint, sideDir) + offset;

			// Add this cell's cosine and sine wave contributions
			phaseDir += vec2(cos(waveInput), sin(waveInput)) * weight;
		}
	}

	vec2  interpolated = phaseDir / weightSum;
	float magnitude = sqrt(dot(interpolated, interpolated));
	magnitude = max(1.0 - normalization, magnitude);
	return vec4(interpolated / magnitude, sideDir);
}

// -----------------------------------------------------------------------------
// EROSION UTILITIES
// -----------------------------------------------------------------------------

float pow_inv(float t, float power) {
	return 1.0 - pow(1.0 - clamp01(t), power);
}

float ease_out(float t) {
	float v = 1.0 - clamp01(t);
	return 1.0 - v * v;
}

float smooth_start(float t, float smoothing) {
	if (t >= smoothing)
		return t - 0.5 * smoothing;
	return 0.5 * t * t / smoothing;
}

vec2 safe_normalize(vec2 n) {
	float l = length(n);
	return (abs(l) > 1e-10) ? (n / l) : n;
}

// -----------------------------------------------------------------------------
// EROSION FILTER
// -----------------------------------------------------------------------------

/**
 * Advanced Terrain Erosion Filter
 *
 * @param p World or UV coordinates for evaluation.
 * @param heightAndSlope Input height (x) and derivative (yz).
 * @param fadeTarget Value to fade towards (-1 at valleys, 1 at peaks).
 * @param strength Overall erosion strength.
 * @param gullyWeight Balance between sharpening and gullies (0 to 1).
 * @param detail Detail restriction (higher = more detail on shallow slopes).
 * @param rounding Rounding parameters (x: ridge, y: crease, z: input mult, w: octave mult).
 * @param onset Controls where erosion takes effect (x: initial, y: octave, z: ridgemap initial, w: ridgemap octave).
 * @param assumedSlope Override slope for initial gully direction (x: value, y: override amount).
 * @param scale Spatial scale of the erosion.
 * @param octaves Number of gully scales to layer.
 * @param lacunarity Frequency multiplier per octave.
 * @param gain Magnitude multiplier per octave.
 * @param cellScale Size of Phacelle noise cells.
 * @param normalization Consistency of gully magnitudes.
 * @param ridgeMap Output: -1 on creases, 1 on ridges.
 * @param substrate Output: -1 for heavy erosion (valleys), 1 for deposition (plains/ridges).
 * @param debug Output: for visualization.
 * @return vec4(heightDelta, slopeDelta.xy, totalMagnitude)
 */
vec4 ErosionFilter(
	in vec2   p,
	vec3      heightAndSlope,
	float     fadeTarget,
	float     strength,
	float     gullyWeight,
	float     detail,
	vec4      rounding,
	vec4      onset,
	vec2      assumedSlope,
	float     scale,
	int       octaves,
	float     lacunarity,
	float     gain,
	float     cellScale,
	float     normalization,
	out float ridgeMap,
	out float substrate,
	out float debug
) {
	strength *= (scale + 10.0);
	fadeTarget = clamp(fadeTarget, -1.0, 1.0);

	scale *= 100.0;

	vec3  inputHeightAndSlope = heightAndSlope;
	float freq = 1.0 / (scale * cellScale);
	float slopeLength = max(length(heightAndSlope.yz), 1e-10);
	float magnitude = 0.0;
	float roundingMult = 1.0;

	float roundingForInput = mix(rounding.y, rounding.x, clamp01(fadeTarget + 0.5)) * rounding.z;
	float combiMask = ease_out(smooth_start(slopeLength * onset.x, roundingForInput * onset.x));

	float ridgeMapCombiMask = ease_out(slopeLength * onset.z);
	float ridgeMapFadeTarget = fadeTarget;

	vec2 gullySlope = mix(heightAndSlope.yz, heightAndSlope.yz / slopeLength * assumedSlope.x, assumedSlope.y);

	for (int i = 0; i < octaves; i++) {
		vec4 phacelle = PhacelleNoise(p * freq, safe_normalize(gullySlope), cellScale, 0.25, normalization);
		phacelle.zw *= -freq;
		float sloping = abs(phacelle.y);

		gullySlope += sign(phacelle.y) * phacelle.zw * strength * gullyWeight;

		vec3 gullies = vec3(phacelle.x, phacelle.y * phacelle.zw);
		vec3 fadedGullies = mix(vec3(fadeTarget, 0.0, 0.0), gullies * gullyWeight, combiMask);
		heightAndSlope += fadedGullies * strength;
		magnitude += strength;

		fadeTarget = fadedGullies.x;

		float roundingForOctave = mix(rounding.y, rounding.x, clamp01(phacelle.x + 0.5)) * roundingMult;
		float newMask = ease_out(smooth_start(sloping * onset.y, roundingForOctave * onset.y));
		combiMask = pow_inv(combiMask, detail) * newMask;

		ridgeMapFadeTarget = mix(ridgeMapFadeTarget, gullies.x, ridgeMapCombiMask);
		float newRidgeMapMask = ease_out(sloping * onset.w);
		ridgeMapCombiMask = ridgeMapCombiMask * newRidgeMapMask;

		strength *= gain;
		freq *= lacunarity;
		roundingMult *= rounding.w;
	}

	ridgeMap = ridgeMapFadeTarget * (1.0 - ridgeMapCombiMask);
	substrate = clamp(fadeTarget, -1.0, 1.0);
	debug = fadeTarget;

	vec3 heightAndSlopeDelta = heightAndSlope - inputHeightAndSlope;
	return vec4(heightAndSlopeDelta, magnitude);
}

// -----------------------------------------------------------------------------
// COLOR MAPPING
// -----------------------------------------------------------------------------

/**
 * Apply realistic erosion color mapping.
 *
 * @param albedo Base terrain color.
 * @param ridgeMap Ridge map output from ErosionFilter (-1 to 1).
 * @param heightDelta Height change from erosion.
 * @param sedimentColor Color of accumulated sediment in creases.
 * @param ridgeColor Color of exposed rock on ridges.
 */
vec3 applyErosionColorMapping(vec3 albedo, float ridgeMap, float heightDelta, vec3 sedimentColor, vec3 ridgeColor) {
	// Darken/Color creases (sediment/water accumulation)
	float creaseMask = smoothstep(0.2, -0.8, ridgeMap);
	albedo = mix(albedo, sedimentColor, creaseMask * 0.5);

	// Highlight ridges (exposed rock/weathering)
	float ridgeMask = smoothstep(0.2, 0.8, ridgeMap);
	albedo = mix(albedo, ridgeColor, ridgeMask * 0.3);

	// Subtle darkening in deeper eroded areas
	albedo *= (1.0 - clamp01(-heightDelta * 2.0) * 0.2);

	return albedo;
}

/**
 * Default color mapping with sensible defaults.
 */
vec3 applyErosionColorMappingDefault(vec3 albedo, float ridgeMap, float heightDelta) {
	vec3 sediment = vec3(0.1, 0.08, 0.05); // Dark dirt/moist soil
	vec3 rock = vec3(0.8, 0.75, 0.7);      // Lighter exposed rock
	return applyErosionColorMapping(albedo, ridgeMap, heightDelta, sediment, rock);
}

#endif // HELPERS_EROSION_GLSL
#endif // GSHADERS_HELPERS_EROSION_GLSL
//END shaders/helpers/erosion.glsl (returning to shaders/terrain.frag)
//START shaders/helpers/fast_noise.glsl
#ifndef GSHADERS_HELPERS_FAST_NOISE_GLSL
#define GSHADERS_HELPERS_FAST_NOISE_GLSL
// Helper functions for fast texture-based noise lookups
// Requires noise texture samplers bound to fixed units:
// u_noiseTexture: 3D, unit 5, R=Simplex/G=Worley/B=FBM/A=Warped
// u_curlTexture: 3D, unit 6, RGB=Curl/A=FBM Curl Mag
// u_blueNoiseTexture: 2D, unit 7, RGBA tiling blue noise at 4 frequencies
// u_extraNoiseTexture: 3D, unit 8, R=Ridge/G=Gradient

uniform sampler3D u_noiseTexture;
uniform sampler3D u_curlTexture;
uniform sampler2D u_blueNoiseTexture;
uniform sampler3D u_extraNoiseTexture;

// R: Simplex 3D
float fastSimplex3d(vec3 p) {
	return texture(u_noiseTexture, p).r * 2.0 - 1.0;
}

// G: Worley 3D
float fastWorley3d(vec3 p) {
	return texture(u_noiseTexture, p).g;
}

// B: FBM 3D
float fastFbm3d(vec3 p) {
	return texture(u_noiseTexture, p).b * 2.0 - 1.0;
}

// A: Warped FBM 3D
float fastWarpedFbm3d(vec3 p) {
	return texture(u_noiseTexture, p).a * 2.0 - 1.0;
}

// Extra Noises (from u_extraNoiseTexture)
// R: Ridge 3D
float fastRidge3d(vec3 p) {
	return texture(u_extraNoiseTexture, p).r;
}

// G: Gradient 3D
float fastGradient3d(vec3 p) {
	return texture(u_extraNoiseTexture, p).g * 2.0 - 1.0;
}

// Multi-octave texture FBM
float fastTextureFbm(vec3 p, int octaves) {
	float value = 0.0;
	float amplitude = 0.5;
	for (int i = 0; i < octaves; i++) {
		value += amplitude * (texture(u_noiseTexture, p).r * 2.0 - 1.0);
		p *= 2.0;
		amplitude *= 0.5;
	}
	return value;
}

// Curl Noise lookup
vec3 fastCurl3d(vec3 p) {
	return texture(u_curlTexture, p).rgb;
}

// FBM Curl magnitude lookup
float fastFbmCurl3d(vec3 p) {
	return texture(u_curlTexture, p).a;
}

// Blue Noise lookups (at different frequencies)
float fastBlueNoise(vec2 uv, int frequencyIndex) {
	vec4 bn = texture(u_blueNoiseTexture, uv);
	if (frequencyIndex == 0)
		return bn.r;
	if (frequencyIndex == 1)
		return bn.g;
	if (frequencyIndex == 2)
		return bn.b;
	return bn.a;
}

float fastBlueNoise(vec2 uv) {
	return texture(u_blueNoiseTexture, uv).r;
}
#endif // GSHADERS_HELPERS_FAST_NOISE_GLSL
//END shaders/helpers/fast_noise.glsl (returning to shaders/terrain.frag)
//START shaders/helpers/lighting.glsl
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

struct Light {
	vec3  position;
	float intensity;
	vec3  color;
	int   type;
	vec3  direction;
	float inner_cutoff; // Also: emissive_radius (EMISSIVE), flash_radius (FLASH)
	float outer_cutoff; // Also: falloff_exp (FLASH)
};

struct AmbientProbe {
	vec4 sh_coeffs[9]; // rgb = coefficients, w = unused
};

const int MAX_LIGHTS = 10;
const int MAX_SHADOW_MAPS = 16;
const int MAX_CASCADES = 4;

layout(std140) uniform Lighting {
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
	vec4  sh_coeffs[9];
};

layout(std430, binding = [[TERRAIN_PROBES_BINDING]]) buffer TerrainProbes {
	AmbientProbe u_terrainProbes[];
};

// Shadow mapping UBO (binding set via glUniformBlockBinding to point 2)
layout(std140) uniform Shadows {
	mat4 lightSpaceMatrices[MAX_SHADOW_MAPS];
	vec4 cascadeSplits;
	int  numShadowLights;
};

// Shadow map texture array - bound to texture unit 4
uniform sampler2DArrayShadow shadowMaps;

// Per-light shadow map index (-1 if no shadow for this light)
// This is set via uniform since the Light struct can't easily store it
uniform int lightShadowIndices[MAX_LIGHTS];

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
//END shaders/helpers/fast_noise.glsl (returning to shaders/helpers/clouds.glsl)
//START shaders/helpers/math.glsl
#ifndef GSHADERS_HELPERS_MATH_GLSL
#define GSHADERS_HELPERS_MATH_GLSL

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

#endif // HELPERS_CLOUDS_GLSL
#endif // GSHADERS_HELPERS_CLOUDS_GLSL
//END shaders/helpers/clouds.glsl (returning to shaders/helpers/lighting.glsl)
//START shaders/helpers/terrain_shadows.glsl
#ifndef GSHADERS_HELPERS_TERRAIN_SHADOWS_GLSL
#define GSHADERS_HELPERS_TERRAIN_SHADOWS_GLSL
#ifndef TERRAIN_SHADOWS_GLSL
#define TERRAIN_SHADOWS_GLSL

//START shaders/helpers/fast_noise.glsl
//END shaders/helpers/fast_noise.glsl (returning to shaders/helpers/terrain_shadows.glsl)

layout(std140, binding = 8) uniform TerrainData {
	ivec4 u_originSize;    // x, y=z, z=size, w=isBound
	vec4  u_terrainParams; // x=chunkSize, y=worldScale
};

uniform isampler2D u_chunkGrid;
uniform sampler2D  u_maxHeightGrid;
// u_heightmapArray is bound to unit 13
uniform sampler2DArray u_heightmapArray;

float getTerrainHeight(vec2 worldXZ) {
	if (u_originSize.w < 1)
		return -10000.0;
	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;
	vec2  gridPos = worldXZ / scaledChunkSize;
	ivec2 chunkCoord = ivec2(floor(gridPos));
	ivec2 localGridCoord = chunkCoord - u_originSize.xy;

	if (localGridCoord.x < 0 || localGridCoord.x >= u_originSize.z || localGridCoord.y < 0 ||
	    localGridCoord.y >= u_originSize.z) {
		return -9999.0; // Debug value
	}

	int slice = texelFetch(u_chunkGrid, localGridCoord, 0).r;
	if (slice < 0)
		return -10000.0;

	vec2 uv = (worldXZ - vec2(chunkCoord) * scaledChunkSize) / scaledChunkSize;
	vec2 remappedUV = (uv * u_terrainParams.x + 0.5) / (u_terrainParams.x + 1.0);
	return texture(u_heightmapArray, vec3(remappedUV, float(slice))).r;
}

/**
 * Perform a coarse raymarch in a specific direction to check for terrain occlusion.
 * Similar to terrainShadowCoverage but optimized for ambient occlusion (AO).
 */
float marchOcclusion(vec3 p_start, vec3 rayDir, float maxDist) {
	float t = 0.0;
	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;

	vec2  gridPos = p_start.xz / scaledChunkSize;
	ivec2 currentChunk = ivec2(floor(gridPos));

	vec2 d = sign(rayDir.xz);
	vec2 safeRayDir = vec2(abs(rayDir.x) < 1e-6 ? 1e-6 : abs(rayDir.x), abs(rayDir.z) < 1e-6 ? 1e-6 : abs(rayDir.z));
	vec2 tDelta = scaledChunkSize / safeRayDir;

	vec2 tMax;
	tMax.x = (d.x > 0.0) ? (floor(gridPos.x) + 1.0 - gridPos.x) * tDelta.x
						 : (gridPos.x - floor(gridPos.x)) * tDelta.x;
	tMax.y = (d.y > 0.0) ? (floor(gridPos.y) + 1.0 - gridPos.y) * tDelta.y
						 : (gridPos.y - floor(gridPos.y)) * tDelta.y;

	int iter = 0;
	while (t < maxDist && iter < 16) { // Very few iterations for AO
		iter++;

		ivec2 localGridCoord = currentChunk - u_originSize.xy;
		if (localGridCoord.x < 0 || localGridCoord.x >= u_originSize.z || localGridCoord.y < 0 ||
		    localGridCoord.y >= u_originSize.z) {
			break;
		}

		float tNext = min(tMax.x, tMax.y);
		float tEnd = min(tNext, maxDist);

		vec2  gridUV = (vec2(localGridCoord) + 0.5) / float(u_originSize.z);
		float h_max = textureLod(u_maxHeightGrid, gridUV, 1.0).r; // Coarse check (mip 1)
		float rayY = p_start.y + t * rayDir.y;

		if (rayY < h_max) {
			// Instead of a full sub-march, we just estimate occlusion based on how much
			// we're below the max height.
			return clamp(1.0 - (h_max - rayY) / (maxDist * 0.5), 0.0, 1.0);
		}

		t = tEnd;
		if (tMax.x < tMax.y) {
			tMax.x += tDelta.x;
			currentChunk.x += int(d.x);
		} else {
			tMax.y += tDelta.y;
			currentChunk.y += int(d.y);
		}
	}

	return 1.0;
}

/**
 * Calculate macro terrain occlusion by sampling in 6 directions around the horizon.
 * Returns [0, 1] where 0 is fully occluded (valley) and 1 is open sky.
 */
float calculateTerrainOcclusion(vec3 worldPos, vec3 normal) {
	if (u_originSize.w < 1)
		return 1.0;

	// Use 6 directions for better horizon coverage
	// Samples at ~30 degrees elevation to capture nearby peaks
	const float h = 0.866; // cos(30)
	const float v = 0.5;   // sin(30)

	vec3 dirs[6] = {
		vec3(h, v, 0.0),
		vec3(-h, v, 0.0),
		vec3(h * 0.5, v, h),
		vec3(-h * 0.5, v, h),
		vec3(h * 0.5, v, -h),
		vec3(-h * 0.5, v, -h)
	};

	float occ = 0.0;
	float maxDist = 300.0 * u_terrainParams.y;
	vec3  p_start = worldPos + normal * (1.5 * u_terrainParams.y); // Lift off surface

	for (int i = 0; i < 6; ++i) {
		occ += marchOcclusion(p_start, dirs[i], maxDist);
	}

	// Ambient Occlusion is the average visibility
	float ao = occ / 6.0;

	// Apply a stronger curve to valleys to increase contrast
	ao = pow(ao, 1.5);

	// Boost for flat/upward surfaces
	return clamp(ao + normal.y * 0.15, 0.0, 1.0);
}

float terrainShadowCoverage(vec3 worldPos, vec3 normal, vec3 lightDir) {
	if (u_originSize.w < 1)
		return 1.0;
	// lightDir is from fragment to light
	float sundownShadow = smoothstep(0.0, 0.02, lightDir.y);
	if (lightDir.y <= 0.02) {
		return sundownShadow;
	}

	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;

	// Better initial bias: move along normal and a bit along light direction.
	// Increased bias and light-dir push to prevent self-shadowing grooves at chunk boundaries.
	vec3  p_start = worldPos + normal * (0.5 * u_terrainParams.y) + lightDir * 2.0;
	float t = 0.0;
	float maxDist = 1200.0 * u_terrainParams.y;

	vec2 rayDir = vec2(lightDir.x, lightDir.z);
	vec2 stepDir = sign(rayDir);
	// Avoid division by zero
	vec2 safeRayDir = vec2(abs(rayDir.x) < 1e-6 ? 1e-6 : abs(rayDir.x), abs(rayDir.y) < 1e-6 ? 1e-6 : abs(rayDir.y));
	vec2 tDelta = scaledChunkSize / safeRayDir;

	vec2  gridPos = p_start.xz / scaledChunkSize;
	ivec2 currentChunk = ivec2(floor(gridPos));

	vec2 tMax;
	tMax.x = (stepDir.x > 0.0) ? (floor(gridPos.x) + 1.0 - gridPos.x) * tDelta.x
							   : (gridPos.x - floor(gridPos.x)) * tDelta.x;
	tMax.y = (stepDir.y > 0.0) ? (floor(gridPos.y) + 1.0 - gridPos.y) * tDelta.y
							   : (gridPos.y - floor(gridPos.y)) * tDelta.y;

	float closest = 1.0;
	int   iter = 0;

	while (t < maxDist && iter < 128) {
		iter++;

		ivec2 localGridCoord = currentChunk - u_originSize.xy;
		if (localGridCoord.x < 0 || localGridCoord.x >= u_originSize.z || localGridCoord.y < 0 ||
		    localGridCoord.y >= u_originSize.z) {
			break; // Out of grid bounds
		}

		float tNext = min(tMax.x, tMax.y);
		float tEnd = min(tNext, maxDist);

		vec2 gridUV = (vec2(localGridCoord) + 0.5) / float(u_originSize.z);

		// Hi-Z Skip: Mip 3 covers 8x8 chunks
		float h_max3 = textureLod(u_maxHeightGrid, gridUV, 3.0).r;
		float rayYAtT = p_start.y + t * lightDir.y;

		if (rayYAtT < h_max3 + 1.0) {
			// Check mip 1 (2x2 chunks)
			float h_max1 = textureLod(u_maxHeightGrid, gridUV, 1.0).r;
			if (rayYAtT < h_max1 + 0.5) {
				// At LOD 0, check actual chunk height
				float h_max0 = textureLod(u_maxHeightGrid, gridUV, 0.0).r;
				if (rayYAtT < h_max0 + 0.25) {
					int slice = texelFetch(u_chunkGrid, localGridCoord, 0).r;
					if (slice >= 0) {
						// Sub-march inside this chunk
						float subT = t;
						float subStep = 1.5 * u_terrainParams.y;
						while (subT < tEnd) {
							vec3  p = p_start + subT * lightDir;
							vec2  uv_chunk = (p.xz - vec2(currentChunk) * scaledChunkSize) / scaledChunkSize;
							vec2  remappedUV = (uv_chunk * u_terrainParams.x + 0.5) / (u_terrainParams.x + 1.0);
							float h = texture(u_heightmapArray, vec3(remappedUV, float(slice))).r;
							if (p.y < h) {
								return 0.0; // Hit terrain!
							}
							closest = min(closest, 8.0 * ((p.y - h) / subT));
							subT += subStep;
						}
					}
				}
			}
		}

		t = tEnd;
		if (t >= maxDist)
			break;

		if (tMax.x < tMax.y) {
			tMax.x += tDelta.x;
			currentChunk.x += int(stepDir.x);
		} else {
			tMax.y += tDelta.y;
			currentChunk.y += int(stepDir.y);
		}
	}

	return closest;
}

bool isPointInTerrainShadow(vec3 worldPos, vec3 normal, vec3 lightDir) {
	return terrainShadowCoverage(worldPos, normal, lightDir) <= 0.0;
}

int isPointInTerrainShadowDebug(vec3 worldPos, vec3 normal, vec3 lightDir) {
	if (u_originSize.w < 1)
		return -3; // Blue
	if (u_terrainParams.y <= 0.0)
		return -1; // Cyan
	if (u_terrainParams.x <= 0.0)
		return -4; // White (Invalid chunkSize)
	if (lightDir.y <= 0.02)
		return -2; // Orange-ish (Light below horizon or too low)

	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;
	vec3  p_start = worldPos + normal * (0.2 * u_terrainParams.y) + lightDir * 1.5;
	float t = 0.0;
	float maxDist = 1200.0 * u_terrainParams.y;

	vec2 rayDir = vec2(lightDir.x, lightDir.z);
	vec2 stepDir = sign(rayDir);
	vec2 safeRayDir = vec2(abs(rayDir.x) < 1e-6 ? 1e-6 : abs(rayDir.x), abs(rayDir.y) < 1e-6 ? 1e-6 : abs(rayDir.y));
	vec2 tDelta = scaledChunkSize / safeRayDir;

	vec2  gridPos = p_start.xz / scaledChunkSize;
	ivec2 currentChunk = ivec2(floor(gridPos));

	vec2 tMax;
	tMax.x = (stepDir.x > 0.0) ? (floor(gridPos.x) + 1.0 - gridPos.x) * tDelta.x
							   : (gridPos.x - floor(gridPos.x)) * tDelta.x;
	tMax.y = (stepDir.y > 0.0) ? (floor(gridPos.y) + 1.0 - gridPos.y) * tDelta.y
							   : (gridPos.y - floor(gridPos.y)) * tDelta.y;

	int iter = 0;
	while (t < maxDist && iter < 128) {
		iter++;

		ivec2 localGridCoord = currentChunk - u_originSize.xy;
		if (localGridCoord.x < 0)
			return 11;
		if (localGridCoord.x >= u_originSize.z)
			return 12;
		if (localGridCoord.y < 0)
			return 13;
		if (localGridCoord.y >= u_originSize.z)
			return 14;

		if (u_originSize.z != 128)
			return 5;

		float tNext = min(tMax.x, tMax.y);
		float tEnd = min(tNext, maxDist);

		vec2  gridUV = (vec2(localGridCoord) + 0.5) / float(u_originSize.z);
		float h_max3 = textureLod(u_maxHeightGrid, gridUV, 3.0).r;
		float rayYAtT = p_start.y + t * lightDir.y;

		if (rayYAtT < h_max3 + 1.0) {
			float h_max1 = textureLod(u_maxHeightGrid, gridUV, 1.0).r;
			if (rayYAtT < h_max1 + 0.5) {
				float h_max0 = textureLod(u_maxHeightGrid, gridUV, 0.0).r;
				if (rayYAtT < h_max0 + 0.25) {
					int slice = texelFetch(u_chunkGrid, localGridCoord, 0).r;
					if (slice < 0) {
						return 2; // No slice (Yellow)
					}

					// Sub-march inside this chunk
					float subT = t;
					float subStep = 1.5 * u_terrainParams.y;
					while (subT < tEnd) {
						vec3  p = p_start + subT * lightDir;
						vec2  uv_chunk = (p.xz - vec2(currentChunk) * scaledChunkSize) / scaledChunkSize;
						float h = texture(u_heightmapArray, vec3(uv_chunk, float(slice))).r;
						if (p.y < h) {
							return 3; // Hit! (Magenta)
						}
						subT += subStep;
					}
				}
			}
		}

		t = tEnd;
		if (t >= maxDist)
			break;

		if (tMax.x < tMax.y) {
			tMax.x += tDelta.x;
			currentChunk.x += int(stepDir.x);
		} else {
			tMax.y += tDelta.y;
			currentChunk.y += int(stepDir.y);
		}
	}

	return 0; // Miss (Green)
}

#endif
#endif // GSHADERS_HELPERS_TERRAIN_SHADOWS_GLSL
//END shaders/helpers/terrain_shadows.glsl (returning to shaders/helpers/lighting.glsl)

// Atmosphere constants for transmittance lookup
const float kEarthRadiusKM = 6360.0;

#ifndef ATMOSPHERE_HEIGHT_DEFINED
	#define ATMOSPHERE_HEIGHT_DEFINED
uniform float u_atmosphereHeight; // usually 100.0 km
#endif

#ifndef TRANSMITTANCE_LUT_DEFINED
	#define TRANSMITTANCE_LUT_DEFINED
uniform sampler2D u_transmittanceLUT;
#endif

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

	// No warp for shadows — warp is a camera viewport trick, shadows should
	// be cast from actual cloud positions
	float weatherWarpFactor = 1.0;

	vec2  weatherUV = cloudPos.xz / (4000.0 * worldScale);
	float weatherMap = weatherWarpFactor * (fastWorley3d(vec3(weatherUV, time * 0.001)) * 0.5 + 0.5);

	vec2  heightUV = cloudPos.xz / (2500.0 * worldScale);
	float heightMap = weatherWarpFactor * (fastWorley3d(vec3(heightUV, time * 0.0004)) * 0.5 + 0.5);

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

	// Sample at the center of the dynamic layer — the fixed projection altitude
	// often falls outside the layer due to altitude offsets from the height map
	cloudPos.y = (layer.baseFloor + layer.baseCeiling) * 0.5;

	float d = calculateCloudShadowDensity(cloudPos, weather, layer, props, time);

	return mix(1.0, exp(-d), cloudShadowIntensity);
}

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
	res += a2 * c4 * coeffs[6].rgb * (3.0 * n.z * n.z - 1.0);
	res += a2 * c5 * coeffs[8].rgb * (n.x * n.x - n.y * n.y);
	return max(res, 0.0);
}

/**
 * Evaluate Spherical Harmonics irradiance for a given normal.
 * Uses 2nd-order SH coefficients from the Lighting UBO.
 */
vec3 evalSHIrradiance(vec3 n) {
	return evalSHIrradianceFromCoeffs(n, sh_coeffs);
}

/**
 * Look up and interpolate Spherical Harmonic ambient irradiance for a fragment.
 */
vec3 getSpatialAmbientSH(vec3 worldPos, vec3 N) {
	if (u_originSize.w < 1)
		return evalSHIrradiance(N);

	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;
	vec2  gridPos = worldPos.xz / scaledChunkSize;
	vec2  fracPos = fract(gridPos);
	ivec2 chunkCoord = ivec2(floor(gridPos)) - u_originSize.xy;

	// Simple bilinear interpolation between 4 nearest chunk probes
	vec3 totalSH[9];
	for (int i = 0; i < 9; ++i)
		totalSH[i] = vec3(0.0);

	float totalWeight = 0.0;
	for (int x = 0; x <= 1; ++x) {
		for (int z = 0; z <= 1; ++z) {
			ivec2 coord = chunkCoord + ivec2(x, z);
			if (coord.x >= 0 && coord.x < u_originSize.z && coord.y >= 0 && coord.y < u_originSize.z) {
				float weight = (x == 0 ? 1.0 - fracPos.x : fracPos.x) * (z == 0 ? 1.0 - fracPos.y : fracPos.y);
				int   idx = coord.y * u_originSize.z + coord.x;
				for (int i = 0; i < 9; ++i) {
					totalSH[i] += u_terrainProbes[idx].sh_coeffs[i].rgb * weight;
				}
				totalWeight += weight;
			}
		}
	}

	if (totalWeight > 0.001) {
		vec4 interpolatedCoeffs[9];
		for (int i = 0; i < 9; ++i) {
			interpolatedCoeffs[i] = vec4(totalSH[i] / totalWeight, 1.0);
		}
		return evalSHIrradianceFromCoeffs(N, interpolatedCoeffs);
	}

	return evalSHIrradiance(N);
}

/**
 * Calculate the relative luminance of a color.
 * Used for determining how much a specular highlight should contribute to fragment opacity.
 */
float get_luminance(vec3 color) {
	return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

// ============================================================================
// PBR Functions (Cook-Torrance BRDF)
// ============================================================================

// Normal Distribution Function (GGX/Trowbridge-Reitz)
float DistributionGGX(vec3 N, vec3 H, float roughness) {
	// Clamp roughness to avoid singularity at 0 (causes black surfaces)
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

// Geometry function (Schlick-GGX)
float GeometrySchlickGGX(float NdotV, float roughness) {
	float r = (roughness + 1.0);
	float k = (r * r) / 8.0;

	float nom = NdotV;
	float denom = NdotV * (1.0 - k) + k;

	return nom / max(denom, 0.0001);
}

// Smith's method for geometry obstruction and shadowing
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx2 = GeometrySchlickGGX(NdotV, roughness);
	float ggx1 = GeometrySchlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}

// Fresnel-Schlick approximation
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
	return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Fresnel-Schlick with roughness - for environment reflections
// Rough surfaces have less pronounced Fresnel effect
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ============================================================================
// Attenuation Helpers for Light Types
// ============================================================================

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

// ============================================================================
// PBR Lighting Functions
// ============================================================================

// PBR intensity multiplier to compensate for energy conservation
// PBR is inherently darker than legacy Phong.
const float PBR_INTENSITY_BOOST = 1.0;

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
vec4 apply_lighting_pbr(vec3 frag_pos, vec3 normal, vec3 albedo, float roughness, float metallic, float ao) {
	vec3 N = normalize(normal);
	vec3 V = normalize(viewPos - frag_pos);

	// Calculate reflectance at normal incidence
	// For dielectrics use 0.04, for metals use albedo color
	vec3 F0 = vec3(0.04);
	F0 = mix(F0, albedo, metallic);

	vec3  Lo = vec3(0.0);
	float spec_lum = 0.0;

	for (int i = 0; i < num_lights; ++i) {
		// Get light direction and attenuation based on light type
		vec3  L;
		float base_attenuation;
		calculateLightContribution(i, frag_pos, L, base_attenuation);

		vec3 H = normalize(V + L);

		// For PBR, we apply intensity boost to compensate for energy conservation
		// Note: directional lights don't use distance attenuation
		float attenuation;
		vec3  atmosphereTransmittance = vec3(1.0);

		if (lights[i].type == LIGHT_TYPE_DIRECTIONAL) {
			attenuation = lights[i].intensity * PBR_INTENSITY_BOOST;

			// Apply atmospheric attenuation for directional lights (Sun/Moon)
			float r = kEarthRadiusKM + (frag_pos.y / (1000.0 * worldScale));
			float mu = L.y;
			atmosphereTransmittance = texture(u_transmittanceLUT, getTransmittanceUV(r, mu)).rgb;
		} else {
			attenuation = (lights[i].intensity * PBR_INTENSITY_BOOST) * base_attenuation;
		}
		vec3 radiance = lights[i].color * attenuation * atmosphereTransmittance;

		// Cook-Torrance BRDF
		float NDF = DistributionGGX(N, H, roughness);
		float G = GeometrySmith(N, V, L, roughness);
		vec3  F = fresnelSchlick(max(dot(H, V), 0.0), F0);

		vec3  numerator = NDF * G * F;
		float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
		vec3  specular = numerator / denominator;

		// kS is Fresnel (specular component)
		vec3 kS = F;
		// kD is diffuse component (energy conservation: kD + kS = 1.0)
		vec3 kD = vec3(1.0) - kS;
		// Metals don't have diffuse lighting
		kD *= 1.0 - metallic;

		float NdotL = max(dot(N, L), 0.0);

		// Calculate shadow with slope-scaled bias
		float shadow = calculateShadow(i, frag_pos, N, L);

		// Apply cloud shadow for directional lights
		if (lights[i].type == LIGHT_TYPE_DIRECTIONAL) {
			shadow *= calculateCloudShadow(i, frag_pos);
		}

		// Add to outgoing radiance Lo
		vec3 specular_radiance = specular * radiance * NdotL * shadow;
		Lo += (kD * albedo / PI) * radiance * NdotL * shadow + specular_radiance;
		spec_lum += get_luminance(specular_radiance);
	}

	// Spatially-varying SH ambient augmented with macro occlusion
	float terrainOcc = calculateTerrainOcclusion(frag_pos, N);
	vec3  spatialSHAmbient = getSpatialAmbientSH(frag_pos, N);

	float combinedAO = ao * terrainOcc;
	vec3  ambientDiffuse = spatialSHAmbient * albedo * combinedAO;

	// Scale down ambient overall to maintain shadow contrast and prevent "flat" look
	ambientDiffuse *= 0.75;

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

/**
 * PBR lighting without shadows - for shaders that don't need shadow calculations.
 * Supports all light types (point, directional, spot).
 * Returns vec4(color.rgb, specular_luminance).
 */
vec4 apply_lighting_pbr_no_shadows(vec3 frag_pos, vec3 normal, vec3 albedo, float roughness, float metallic, float ao) {
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
vec4 apply_lighting(vec3 frag_pos, vec3 normal, vec3 albedo, float specular_strength) {
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
vec4 apply_lighting_no_shadows(vec3 frag_pos, vec3 normal, vec3 albedo, float specular_strength) {
	vec3  result = ambient_light * albedo;
	float spec_lum = 0.0;

	for (int i = 0; i < num_lights; ++i) {
		vec3  light_dir;
		float attenuation;
		calculateLightContribution(i, frag_pos, light_dir, attenuation);

		// Atmospheric attenuation for directional light
		vec3  atmosphereTransmittance = vec3(1.0);
		float shadow = 1.0;
		if (lights[i].type == LIGHT_TYPE_DIRECTIONAL) {
			float r = kEarthRadiusKM + (frag_pos.y / (1000.0 * worldScale));
			float mu = light_dir.y;
			atmosphereTransmittance = texture(u_transmittanceLUT, getTransmittanceUV(r, mu)).rgb;

			// Apply cloud shadow
			shadow *= calculateCloudShadow(i, frag_pos);
		}

		// Diffuse
		float diff = max(dot(normal, light_dir), 0.0);
		vec3  diffuse = lights[i].color * atmosphereTransmittance * diff * albedo;

		// Specular
		vec3  view_dir = normalize(viewPos - frag_pos);
		vec3  reflect_dir = reflect(-light_dir, normal);
		float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32);
		vec3  specular_contribution = lights[i].color * atmosphereTransmittance * spec * specular_strength *
			lights[i].intensity * shadow * attenuation;

		result += (diffuse * lights[i].intensity * shadow * attenuation) + specular_contribution;
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
	float iridescence_strength
) {
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

		// Apply cloud shadow for directional lights
		float shadow = 1.0;
		if (lights[i].type == LIGHT_TYPE_DIRECTIONAL) {
			shadow *= calculateCloudShadow(i, frag_pos);
		}

		vec3 specular_contribution = specular * radiance * NdotL * shadow;
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
	float emissive_coverage
) {
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
		lit_surface = apply_lighting_no_shadows(frag_pos, normal, base_albedo, 0.5);
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
	float emissive_mask
) {
	vec3 N = normalize(normal);
	vec3 V = normalize(viewPos - frag_pos);

	// Emissive component
	vec3  emission = emissive_color * emissive_intensity;
	float fresnel = pow(1.0 - max(dot(N, V), 0.0), 3.0);
	emission += emissive_color * fresnel * emissive_intensity * 0.3;

	// PBR lit component for non-emissive parts
	vec4 pbr_lit = apply_lighting_pbr_no_shadows(frag_pos, normal, base_albedo, roughness, metallic, 1.0);

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
//END shaders/helpers/lighting.glsl (returning to shaders/terrain.frag)
//START shaders/helpers/terrain_noise.glsl
#ifndef GSHADERS_HELPERS_TERRAIN_NOISE_GLSL
#define GSHADERS_HELPERS_TERRAIN_NOISE_GLSL
#ifndef TERRAIN_NOISE_GLSL
#define TERRAIN_NOISE_GLSL

vec3 mod289(vec3 x) {
	return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 mod289(vec4 x) {
	return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 permute(vec4 x) {
	return mod289(((x * 34.0) + 1.0) * x);
}

vec4 taylorInvSqrt(vec4 r) {
	return 1.79284291400159 - 0.85373472095314 * r;
}

float snoise(vec3 v) {
	const vec2 C = vec2(1.0 / 6.0, 1.0 / 3.0);
	const vec4 D = vec4(0.0, 0.5, 1.0, 2.0);

	// First corner
	vec3 i = floor(v + dot(v, C.yyy));
	vec3 x0 = v - i + dot(i, C.xxx);

	// Other corners
	vec3 g = step(x0.yzx, x0.xyz);
	vec3 l = 1.0 - g;
	vec3 i1 = min(g.xyz, l.zxy);
	vec3 i2 = max(g.xyz, l.zxy);

	vec3 x1 = x0 - i1 + C.xxx;
	vec3 x2 = x0 - i2 + C.yyy;
	vec3 x3 = x0 - D.yyy;

	// Permutations
	i = mod289(i);
	vec4 p = permute(
		permute(permute(i.z + vec4(0.0, i1.z, i2.z, 1.0)) + i.y + vec4(0.0, i1.y, i2.y, 1.0)) + i.x +
		vec4(0.0, i1.x, i2.x, 1.0)
	);

	float n_ = 0.142857142857;
	vec3  ns = n_ * D.wyz - D.xzx;

	vec4 j = p - 49.0 * floor(p * ns.z * ns.z);

	vec4 x_ = floor(j * ns.z);
	vec4 y_ = floor(j - 7.0 * x_);

	vec4 x = x_ * ns.x + ns.yyyy;
	vec4 y = y_ * ns.x + ns.yyyy;
	vec4 h = 1.0 - abs(x) - abs(y);

	vec4 b0 = vec4(x.xy, y.xy);
	vec4 b1 = vec4(x.zw, y.zw);

	vec4 s0 = floor(b0) * 2.0 + 1.0;
	vec4 s1 = floor(b1) * 2.0 + 1.0;
	vec4 sh = -step(h, vec4(0.0));

	vec4 a0 = b0.xzyw + s0.xzyw * sh.xxyy;
	vec4 a1 = b1.xzyw + s1.xzyw * sh.zzww;

	vec3 p0 = vec3(a0.xy, h.x);
	vec3 p1 = vec3(a0.zw, h.y);
	vec3 p2 = vec3(a1.xy, h.z);
	vec3 p3 = vec3(a1.zw, h.w);

	// Normalise gradients
	vec4 norm = taylorInvSqrt(vec4(dot(p0, p0), dot(p1, p1), dot(p2, p2), dot(p3, p3)));
	p0 *= norm.x;
	p1 *= norm.y;
	p2 *= norm.z;
	p3 *= norm.w;

	// Mix final noise value
	vec4 m = max(0.6 - vec4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.0);
	m = m * m;
	return 42.0 * dot(m * m, vec4(dot(p0, x0), dot(p1, x1), dot(p2, x2), dot(p3, x3)));
}

float fbm(vec3 p) {
	float value = 0.0;
	float amplitude = 0.5;
	for (int i = 0; i < 4; i++) {
		value += amplitude * snoise(p);
		p *= 2.0;
		amplitude *= 0.5;
	}
	return value;
}

// Higher octave FBM for fine detail
float[6] fbm_detail(vec3 p) {
	float values[6];
	float value = 0.0;
	float amplitude = 0.5;
	for (int i = 0; i < 6; i++) {
		value += amplitude * snoise(p);
		p *= 2.0;
		amplitude *= 0.5;
		values[i] = value;
	}
	return values;
}

// Standard 2D hash for pseudo-random impulse generation
vec2 hash22(vec2 p) {
	p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
	return fract(sin(p) * 43758.5453123);
}

// pos: The world or UV coordinate being shaded
// curlVec: The sampled vector from curl texture at 'pos'
// time: Animation time to drive the wind ripples
// freq: The spatial frequency of the ripples inside the gust
// bandwidth: Controls the tightness of the Gaussian envelope (higher = smaller/sharper gusts)
// sparsity: Threshold to cull impulses (0.0 to 1.0, higher = sparser gusts)
float gaborWindNoise(vec2 pos, vec2 curlVec, float time, float freq, float bandwidth, float sparsity) {
	vec2 gridId = floor(pos);
	vec2 gridFract = fract(pos);

	float noiseAcc = 0.0;

	// Normalize the sampled curl vector to strictly control the ripple direction
	vec2 dir = length(curlVec) > 0.001 ? normalize(curlVec) : vec2(1.0, 0.0);
	vec2 F = dir * freq;

	// 3x3 grid traversal to find neighboring impulses
	for (int y = -1; y <= 1; y++) {
		for (int x = -1; x <= 1; x++) {
			vec2 neighborOffset = vec2(float(x), float(y));
			vec2 cellId = gridId + neighborOffset;

			// Generate random properties for this cell's impulse
			vec2 randVal = hash22(cellId);

			// Check for sparsity: skip this cell entirely if it doesn't meet the threshold
			if (randVal.y < sparsity)
				continue;

			// Calculate vector from fragment to the random impulse position in the neighbor cell
			vec2 impulsePos = neighborOffset + randVal;
			vec2 p = gridFract - impulsePos;

			// Distance squared for the Gaussian envelope
			float distSq = dot(p, p);

			// 1. Evaluate the Gaussian Envelope
			// e^(-pi * a^2 * d^2)
			float envelope = exp(-3.14159 * bandwidth * bandwidth * distSq);

			// 2. Evaluate the Harmonic Carrier
			// Animate phase with time, offset randomly per cell so they don't pulse synchronously
			float phase = (time * 5.0) + (randVal.x * 6.28318);
			float carrier = cos(6.28318 * dot(F, p) + phase);

			// Accumulate the result, scaling intensity by the random value for variety
			noiseAcc += randVal.y * envelope * carrier;
		}
	}

	return noiseAcc;
}

float tangentGabor(
	vec3  worldPos,
	vec3  worldNormal,
	vec3  curlVec3D,
	float time,
	float freq,
	float bandwidth,
	float sparsity
) {
	// 1. Construct the TBN frame
	vec3 N = normalize(worldNormal);

	// Choose an 'up' vector, switching to X-axis if the normal is perfectly vertical
	vec3 referenceUp = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);

	vec3 T = normalize(cross(referenceUp, N));
	vec3 B = normalize(cross(N, T));

	// 2. Project world position into 2D surface space
	vec2 surfacePos = vec2(dot(worldPos, T), dot(worldPos, B));

	// 3. Project 3D wind curl vector into 2D surface space
	vec2 surfaceCurl = vec2(dot(curlVec3D, T), dot(curlVec3D, B));

	// 4. Evaluate the 2D Gabor Noise
	return gaborWindNoise(surfacePos, surfaceCurl, time, freq, bandwidth, sparsity);
}

#endif // TERRAIN_NOISE_GLSL
#endif // GSHADERS_HELPERS_TERRAIN_NOISE_GLSL
//END shaders/helpers/terrain_noise.glsl (returning to shaders/terrain.frag)
//START shaders/visual_effects.glsl
#ifndef GSHADERS_VISUAL_EFFECTS_GLSL
#define GSHADERS_VISUAL_EFFECTS_GLSL
#ifndef VISUAL_EFFECTS_GLSL
#define VISUAL_EFFECTS_GLSL

layout(std140) uniform VisualEffects {
	int   ripple_enabled;
	int   color_shift_enabled;
	int   black_and_white_enabled;
	int   negative_enabled;
	int   shimmery_enabled;
	int   glitched_enabled;
	int   wireframe_enabled;
	int   erosion_enabled;
	float wind_strength;
	float wind_speed;
	float wind_frequency;
	float erosion_strength;
	float erosion_scale;
	float erosion_detail;
	float erosion_gully_weight;
	float erosion_max_dist;
};

#endif
#endif // GSHADERS_VISUAL_EFFECTS_GLSL
//END shaders/visual_effects.glsl (returning to shaders/terrain.frag)
// #include "helpers/noise.glsl"

uniform bool uIsShadowPass = false;

// Biome texture array: RG8 - R=low_idx, G=t
uniform sampler2DArray uBiomeMap;
uniform float          uRawChunkSize;

struct BiomeProperties {
	vec4 albedo_roughness; // rgb = albedo, w = roughness
	vec4 params;           // x = metallic, y = detailStrength, z = detailScale, w = unused
};

layout(std140, binding = 7) uniform BiomeData {
	BiomeProperties biomes[8];
};

#define HEIGHT_BEACH_END (3.0 * worldScale)
#define HEIGHT_LOWLAND_END (20.0 * worldScale)
#define HEIGHT_FOREST_END (50.0 * worldScale)
#define HEIGHT_ALPINE_START (60.0 * worldScale)
#define HEIGHT_TREELINE (80.0 * worldScale)
#define HEIGHT_SNOW_START (90.0 * worldScale)
#define HEIGHT_PEAK (100.0 * worldScale)

// Color palette - realistic terrain tones
const vec3 COL_SAND_WET = vec3(0.55, 0.45, 0.35);      // Wet sand near water
const vec3 COL_SAND_DRY = vec3(0.76, 0.70, 0.55);      // Dry beach sand
const vec3 COL_GRASS_LUSH = vec3(0.20, 0.45, 0.15);    // Lush valley grass
const vec3 COL_GRASS_DRY = vec3(0.45, 0.50, 0.25);     // Drier upland grass
const vec3 COL_FOREST = vec3(0.12, 0.28, 0.10);        // Dense forest
const vec3 COL_ALPINE_MEADOW = vec3(0.35, 0.45, 0.25); // High alpine grass
const vec3 COL_ROCK_BROWN = vec3(0.35, 0.30, 0.25);    // Brown cliff rock
const vec3 COL_ROCK_GREY = vec3(0.45, 0.45, 0.48);     // Grey mountain rock
const vec3 COL_ROCK_DARK = vec3(0.25, 0.23, 0.22);     // Dark wet rock
const vec3 COL_SNOW_FRESH = vec3(0.95, 0.97, 1.00);    // Fresh snow
const vec3 COL_SNOW_OLD = vec3(0.85, 0.88, 0.92);      // Older packed snow
const vec3 COL_DIRT = vec3(0.35, 0.25, 0.18);          // Exposed dirt

struct TerrainMaterial {
	vec3  albedo;
	float roughness;
	float metallic;
	float normalScale;
	float normalStrength;
};

/**
 * Calculate valley/ridge factor using noise-based curvature approximation.
 * Valleys tend to accumulate moisture and be more lush.
 * Returns: negative = valley (concave), positive = ridge (convex)
 */
float calculateValleyFactor(vec3 pos) {
	// Sample noise at different scales to approximate local curvature
	float scale = 0.02;
	float center = length(pos * scale);

	// Sample neighbors
	float dx = 5.0;
	float north = length((pos + vec3(0, 0, dx)) * scale);
	float south = length((pos - vec3(0, 0, dx)) * scale);
	float east = length((pos + vec3(dx, 0, 0)) * scale);
	float west = length((pos - vec3(dx, 0, 0)) * scale);

	// Laplacian approximation - negative means we're lower than surroundings (valley)
	float laplacian = (north + south + east + west) / 4.0 - center;

	return laplacian * 10.0; // Scale for usability
}

/**
 * Calculate moisture based on height, valley factor, and noise
 */
float calculateMoisture(float height, float valleyFactor, vec3 pos) {
	// Base moisture decreases with altitude (less rain at high elevations)
	float baseMoisture = 1.0 - smoothstep(0.0, HEIGHT_PEAK, height) * 0.6;

	// Valleys are more moist (water collects there)
	float valleyMoisture = clamp(-valleyFactor * 0.5, 0.0, 0.4);

	// Add some noise variation
	float noiseMoisture = snoise(pos * 0.03) * 0.2;

	return clamp(baseMoisture + valleyMoisture + noiseMoisture, 0.0, 1.0);
}

/**
 * Get the base biome material based on height
 */
TerrainMaterial getBiomeMaterial(float height, float moisture, float noise) {
	TerrainMaterial mat;
	mat.metallic = 0.0;
	// Distort height with noise for natural boundaries
	float h = height + noise * 8.0;

	// Beach zone (0 - 3)
	if (h < HEIGHT_BEACH_END) {
		float wetness = 1.0 - smoothstep(0.0, HEIGHT_BEACH_END, h);
		mat.albedo = mix(COL_SAND_DRY, COL_SAND_WET, wetness);
		mat.roughness = mix(0.9, 0.4, wetness);
		mat.normalScale = 40.0;
		mat.normalStrength = mix(0.1, 0.05, wetness);
		return mat;
	}

	// Lowland zone (3 - 25): grass/meadow, lusher in valleys
	if (h < HEIGHT_LOWLAND_END) {
		float t = smoothstep(HEIGHT_BEACH_END, HEIGHT_LOWLAND_END, h);
		vec3  grassColor = mix(COL_GRASS_LUSH, COL_GRASS_DRY, t * (1.0 - moisture));
		float grassRoughness = mix(0.7, 0.8, t * (1.0 - moisture));
		// Blend from sand to grass
		float sandFade = smoothstep(HEIGHT_BEACH_END, HEIGHT_BEACH_END + 5.0, h);
		mat.albedo = mix(COL_SAND_DRY, grassColor, sandFade);
		mat.roughness = mix(0.9, grassRoughness, sandFade);
		mat.normalScale = mix(40.0, 12.0, sandFade);
		mat.normalStrength = mix(0.1, 0.08, sandFade);
		return mat;
	}

	// Forest zone (25 - 80): trees dominate
	if (h < HEIGHT_FOREST_END) {
		float t = smoothstep(HEIGHT_LOWLAND_END, HEIGHT_FOREST_END, h);
		// More moisture = denser forest
		vec3 forestColor = mix(COL_GRASS_LUSH, COL_FOREST, moisture);
		mat.albedo = mix(forestColor, COL_GRASS_DRY, t * 0.3);
		mat.roughness = mix(0.8, 0.85, t * 0.3);
		mat.normalScale = mix(12.0, 10.0, t);
		mat.normalStrength = mix(0.08, 0.12, t);
		return mat;
	}

	// Transition to alpine (80 - 100)
	if (h < HEIGHT_ALPINE_START) {
		float t = smoothstep(HEIGHT_FOREST_END, HEIGHT_ALPINE_START, h);
		mat.albedo = mix(COL_FOREST, COL_ALPINE_MEADOW, t);
		mat.roughness = 0.8;
		mat.normalScale = mix(10.0, 15.0, t);
		mat.normalStrength = mix(0.12, 0.1, t);
		return mat;
	}

	// Alpine meadow (100 - 130): above treeline
	if (h < HEIGHT_TREELINE) {
		float t = smoothstep(HEIGHT_ALPINE_START, HEIGHT_TREELINE, h);
		// Grass becomes sparser, more rock showing through
		mat.albedo = mix(COL_ALPINE_MEADOW, COL_ROCK_GREY, t * 0.4);
		mat.roughness = mix(0.8, 0.6, t * 0.4);
		mat.normalScale = mix(15.0, 4.0, t * 0.4);
		mat.normalStrength = mix(0.1, 0.2, t * 0.4);
		return mat;
	}

	// High alpine / rocky (130 - 160)
	if (h < HEIGHT_SNOW_START) {
		float t = smoothstep(HEIGHT_TREELINE, HEIGHT_SNOW_START, h);
		// Mostly rock with patches of hardy vegetation
		vec3 rockColor = mix(COL_ROCK_BROWN, COL_ROCK_GREY, noise * 0.5 + 0.5);
		vec3 patchColor = mix(rockColor, COL_ALPINE_MEADOW, moisture * 0.3);
		mat.albedo = mix(patchColor, COL_SNOW_OLD, t * 0.3);
		mat.roughness = mix(0.6, 0.5, t * 0.3);
		mat.normalScale = mix(4.0, 25.0, t * 0.3);
		mat.normalStrength = mix(0.2, 0.05, t * 0.3);
		return mat;
	}

	// Snow zone (160+)
	float t = smoothstep(HEIGHT_SNOW_START, HEIGHT_PEAK, h);
	// Higher = fresher/whiter snow
	vec3 snowColor = mix(COL_SNOW_OLD, COL_SNOW_FRESH, t);
	// Some rock still pokes through at lower snow zone
	float rockShow = (1.0 - t) * 0.2 * (1.0 - moisture);
	mat.albedo = mix(snowColor, COL_ROCK_GREY, rockShow);
	mat.roughness = mix(0.5, 0.4, t);
	mat.normalScale = mix(25.0, 30.0, t);
	mat.normalStrength = mix(0.05, 0.03, t);
	return mat;
}

/**
 * Calculate cliff/steep surface material properties
 * Steep surfaces are barren rock, with properties varying by altitude
 */
TerrainMaterial getCliffMaterial(float height, float noise) {
	TerrainMaterial mat;
	mat.metallic = 0.0;
	float h = height + noise * 5.0;

	// Low altitude cliffs: brown/dark rock (often wet)
	if (h < HEIGHT_FOREST_END) {
		float wetness = 0.3 + noise * 0.2;
		mat.albedo = mix(COL_ROCK_BROWN, COL_ROCK_DARK, wetness);
		mat.roughness = mix(0.6, 0.3, wetness);
		mat.normalScale = 4.0;
		mat.normalStrength = 0.2;
		return mat;
	}

	// Mid altitude: mixed brown/grey
	if (h < HEIGHT_SNOW_START) {
		float t = smoothstep(HEIGHT_FOREST_END, HEIGHT_SNOW_START, h);
		mat.albedo = mix(COL_ROCK_BROWN, COL_ROCK_GREY, t + noise * 0.2);
		mat.roughness = 0.6;
		mat.normalScale = 3.5;
		mat.normalStrength = 0.2;
		return mat;
	}

	// High altitude cliffs: grey rock with snow patches
	float snowPatch = smoothstep(HEIGHT_SNOW_START, HEIGHT_PEAK, h) * 0.4;
	vec3  highRock = mix(COL_ROCK_GREY, COL_ROCK_DARK, noise * 0.3);
	mat.albedo = mix(highRock, COL_SNOW_OLD, snowPatch);
	mat.roughness = mix(0.6, 0.5, snowPatch);
	mat.normalScale = 3.0;
	mat.normalStrength = 0.15;
	return mat;
}

void main() {
	if (uIsShadowPass) {
		// Output only depth (handled by hardware)
		return;
	}

	// Calculate screen-space velocity
	vec2 a = (CurPosition.xy / CurPosition.w) * 0.5 + 0.5;
	vec2 b = (PrevPosition.xy / PrevPosition.w) * 0.5 + 0.5;
	Velocity = a - b;

	// Distance Fade -- precalc
	vec3  norm = normalize(Normal);
	float baseFreq = 0.1 / worldScale;
	float slope = dot(norm, vec3(0.0, 1.0, 0.0));
	vec3  scaledFragPos = FragPos / worldScale;

	float dist = length(FragPos.xz - viewPos.xz);
	// float n_fade = snoise(vec3(FragPos.xy / (25 * worldScale), time * 0.08));
	float n_fade = fastSimplex3d(vec3(FragPos.xz / (250 * worldScale), time * 0.09));
	float fade_start = 560.0 * worldScale;
	float fade_end = 570.0 * worldScale;
	float fade = 1.0 - smoothstep(fade_start, fade_end, dist + n_fade * 40.0);

	if (fade < 0.2) {
		discard;
	}

	if (vIsWater > 0.5) {
		// --- Grid logic (from plane.frag, with refraction) ---
		float grid_spacing = 1.0;

		// Awareness of water surface: ripple depth is FragPos.y
		// (Water is at y=0, ripples go up/down from there)
		float rippleHeight = FragPos.y;

		// Calculate refraction offset based on surface normal and depth (distance from y=0)
		// Since water is fixed at 0, absolute depth is abs(rippleHeight)
		// The more the normal deviates from up (0,1,0), the more refraction
		vec2 refractionOffset = norm.xz * abs(rippleHeight) * 4.0;
		if (dist <= 75.0) {
			refractionOffset = fastCurl3d(vec3(norm.xz / 100.0, rippleHeight)).xz * abs(rippleHeight) * 2.0 *
				smoothstep(75.0, 50.0, dist);
		}
		vec2 coord = (FragPos.xz + refractionOffset) / grid_spacing;
		vec2 f = fwidth(coord);

		vec2  grid_minor = abs(fract(coord - 0.5) - 0.5) / f;
		float line_minor = min(grid_minor.x, grid_minor.y);
		float C_minor = 1.0 - min(line_minor, 1.0);

		vec2  grid_major = abs(fract(coord / 5.0 - 0.5) - 0.5) / f;
		float line_major = min(grid_major.x, grid_major.y);
		float C_major = 1.0 - min(line_major, 1.0);

		// Add shimmer to grid intensity based on ripple height
		float shimmer = 1.0 + rippleHeight * 2.0;
		float grid_intensity = max(C_minor, C_major * 1.5) * 0.6 * shimmer;
		vec3  grid_color = vec3(0.0, 0.8, 0.8) * grid_intensity;

		vec3 surfaceColor = vec3(0.05, 0.05, 0.08);

		// Highly reflective PBR material
		vec3 lighting = apply_lighting_pbr(FragPos, norm, surfaceColor, 0.05, 0.9, 1.0).rgb;
		vec3 final_color = lighting + grid_color;

		// Distance fade and distant cyan blend (matching terrain style)
		vec4 baseColor = vec4(final_color, fade);
		FragColor = mix(vec4(0.0, 0.7, 0.7, baseColor.a) * length(baseColor), baseColor, step(1.0, fade));
		return;
	}

	// ========================================================================
	// Noise Generation
	// ========================================================================
	// Scale world-space position for detail noise to match terrain scaling
	// float largeNoise =    mix(fastFbm3d(FragPos * (baseFreq * 5.0)), fastWarpedFbm3d(FragPos * (baseFreq * 0.5)),
	// fastWorley3d(FragPos * (baseFreq * 0.1)));
	float largeNoise = fastWarpedFbm3d(FragPos * (baseFreq * 0.1));
	float medNoise = largeNoise;
	float fineNoise = largeNoise;
	float macroNoise = largeNoise;
	float combinedNoise = largeNoise;

	float distanceFactor = dist * smoothstep(0, 10.0, FragPos.y);
	float noseFade = fade_start - 100.0;
	/*
	    if (distanceFactor < noseFade) {
	        largeNoise = fastWarpedFbm3d(FragPos * (baseFreq * 0.5));
	        medNoise = fastWorley3d(vec3(largeNoise) * (baseFreq * 2.0));
	        fineNoise = fastFbm3d(FragPos * (baseFreq * 5.0));
	        macroNoise = fastSimplex3d(FragPos * (baseFreq * 0.1));
	        combinedNoise = largeNoise * 0.6 + (1.0 - medNoise) * 0.3 + fineNoise * 0.1;

	        if (distanceFactor > 250) {
	            float angle = 1.0 - dot(viewDir, viewPos - FragPos);
	            largeNoise = mix(largeNoise, 1.0, angle * smoothstep(noseFade, fade_end, distanceFactor));
	            medNoise = mix(medNoise, 1.0, angle * smoothstep(noseFade, fade_end, distanceFactor));
	            fineNoise = mix(fineNoise, 1.0, angle * smoothstep(noseFade, fade_end, distanceFactor));
	            macroNoise = mix(macroNoise, 1.0, angle * smoothstep(noseFade, fade_end, distanceFactor));
	            combinedNoise = mix(combinedNoise, 1.0, angle * smoothstep(noseFade, fade_end, distanceFactor));
	        }
	    }
	*/

	TerrainMaterial finalMaterial;
	// ========================================================================
	// Material Calculation
	// ========================================================================

	// Height with noise distortion for natural boundaries
	float baseHeight = FragPos.y;
	float distortedHeight = baseHeight + largeNoise * 5.0 * worldScale;

	// Slope analysis: 1.0 = flat horizontal, 0.0 = vertical cliff
	float distortedSlope = slope + medNoise * 0.08;

	// // Slope-based cliff blending
	float verticalMask = smoothstep(0.4, 0.2, slope);

	// Valley/ridge detection
	float valleyFactor = calculateValleyFactor(FragPos);

	// Moisture calculation
	float moisture = calculateMoisture(baseHeight, valleyFactor, FragPos);

	// Valley lushness boost
	float valleyLushness = clamp(-valleyFactor, 0.0, 1.0);
	moisture = mix(moisture, min(moisture + 0.3, 1.0), valleyLushness);

	// Cliff mask: steep surfaces become rocky
	// Threshold varies with altitude (snow sticks to steeper surfaces at high alt)
	// Lower threshold = only steeper surfaces become cliffs (0.5 = ~60° from horizontal)
	float cliffThreshold = mix(0.4, 0.3, smoothstep(HEIGHT_SNOW_START, HEIGHT_PEAK, baseHeight));
	float cliffMask = smoothstep(cliffThreshold, cliffThreshold - 0.15, distortedSlope);

	// Near-vertical surfaces (slope < 0.2, ~78° from horizontal) are always cliff-like
	// float verticalMask = smoothstep(0.25, 0.1, slope);
	cliffMask = max(cliffMask, verticalMask);

	// Add noise to cliff boundaries for natural look
	cliffMask += (medNoise - 0.5) * 0.15;
	cliffMask = clamp(cliffMask, 0.0, 1.0);

	// Don't make beach areas into cliffs
	float beachMask = 1.0 - smoothstep(0.0, HEIGHT_BEACH_END + 2.0, baseHeight);
	cliffMask *= (1.0 - beachMask);

	TerrainMaterial biomeMat = getBiomeMaterial(distortedHeight, moisture, combinedNoise);
	TerrainMaterial cliffMat = getCliffMaterial(baseHeight, medNoise);

	// Substrate-based blending: eroded substrate tends to be rockier, while
	// deposition substrate (plains/ridges) can be lusher.
	float substrateCliffFactor = smoothstep(0.2, -0.6, vSubstrate);
	cliffMask = clamp(cliffMask + substrateCliffFactor * 0.4, 0.0, 1.0);

	// Blend biome with cliff material
	finalMaterial.albedo = mix(biomeMat.albedo, cliffMat.albedo, cliffMask);
	finalMaterial.roughness = mix(biomeMat.roughness, cliffMat.roughness, cliffMask);
	finalMaterial.metallic = mix(biomeMat.metallic, cliffMat.metallic, cliffMask);
	finalMaterial.normalScale = mix(biomeMat.normalScale, cliffMat.normalScale, cliffMask);
	finalMaterial.normalStrength = mix(biomeMat.normalStrength, cliffMat.normalStrength, cliffMask);

	// Large-scale macro color shifts
	finalMaterial.albedo *= (1.0 + macroNoise * 0.12);

	// Add subtle color variation based on combined noise
	finalMaterial.albedo *= (1.0 + combinedNoise * 0.15);

	// Extra variety for rocky/steep areas to complement normals
	float rockyVar = fineNoise;
	float rockyMask = smoothstep(0.5, 0.2, slope); // More variety on steeper slopes
	finalMaterial.albedo = mix(finalMaterial.albedo, finalMaterial.albedo * (1.0 + rockyVar * 0.2), rockyMask);

	// ========================================================================
	// Advanced Erosion Filter Coloration
	// ========================================================================
	// Apply the extracted color mapping using the data passed from the tessellation shader
	finalMaterial.albedo = applyErosionColorMappingDefault(finalMaterial.albedo, vRidgeMap, vErosionDelta);

	vec3  albedo = finalMaterial.albedo;
	float roughness = finalMaterial.roughness;
	float metallic = finalMaterial.metallic;
	float normalStrength = finalMaterial.normalStrength;
	float normalScale = finalMaterial.normalScale;

	// ========================================================================
	// Detail Variation
	// ========================================================================

	// ========================================================================
	// Normal Perturbation (Grain)
	// ========================================================================
	vec3 perturbedNorm = norm;

	if (perturbFactor >= 0.1 && normalStrength > 0.0) {
		float roughnessStrength = smoothstep(0.1, 1.0, perturbFactor) * normalStrength;
		float roughnessScale = normalScale * 0.05;
		vec3  scaledFragPos = FragPos / worldScale;

		// Sample biome noise type (using unused params.w)
		vec2  biomeUV = (TexCoords * uRawChunkSize + 0.5) / (uRawChunkSize + 1.0);
		vec2  biomeInfo = texture(uBiomeMap, vec3(biomeUV, TextureSlice)).rg;
		float noiseTypeA = biomes[int(biomeInfo.x)].params.w;
		float noiseTypeB = biomes[min(int(biomeInfo.x) + 1, 7)].params.w;
		float noiseType = mix(noiseTypeA, noiseTypeB, biomeInfo.y);

		// Use finite difference to approximate the gradient of the noise field
		float eps = 0.015;
		float n, nx, nz;

		if (noiseType < 0.5) { // Simplex (0)
			n = fastSimplex3d(0.1 * scaledFragPos * roughnessScale);
			nx = fastSimplex3d(0.1 * (scaledFragPos + vec3(eps, 0.0, 0.0)) * roughnessScale);
			nz = fastSimplex3d(0.1 * (scaledFragPos + vec3(0.0, 0.0, eps)) * roughnessScale);
		} else if (noiseType < 1.5) { // Worley (1)
			n = fastWorley3d(0.1 * scaledFragPos * roughnessScale);
			nx = fastWorley3d(0.1 * (scaledFragPos + vec3(eps, 0.0, 0.0)) * roughnessScale);
			nz = fastWorley3d(0.1 * (scaledFragPos + vec3(0.0, 0.0, eps)) * roughnessScale);
		} else if (noiseType < 2.5) { // FBM (2)
			n = fastFbm3d(0.1 * scaledFragPos * roughnessScale);
			nx = fastFbm3d(0.1 * (scaledFragPos + vec3(eps, 0.0, 0.0)) * roughnessScale);
			nz = fastFbm3d(0.1 * (scaledFragPos + vec3(0.0, 0.0, eps)) * roughnessScale);
		} else { // Ridge (3)
			n = fastRidge3d(0.1 * scaledFragPos * roughnessScale);
			nx = fastRidge3d(0.1 * (scaledFragPos + vec3(eps, 0.0, 0.0)) * roughnessScale);
			nz = fastRidge3d(0.1 * (scaledFragPos + vec3(0.0, 0.0, eps)) * roughnessScale);
		}

		// Compute local tangent space to orient the perturbation.
		// Using a stable basis that doesn't flip at Z-axis alignment.
		vec3 v = abs(norm.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
		vec3 tangent = normalize(cross(v, norm));
		vec3 bitangent = cross(norm, tangent);

		// Apply perturbation based on noise gradient
		perturbedNorm = normalize(norm + (tangent * (n - nx) + bitangent * (n - nz)) * (roughnessStrength / eps));

		// Toksvig Factor: Adjust roughness based on normal length after interpolation
		float ft = length(perturbedNorm);
		ft = clamp(ft, 0.01, 1.0);
		float r2 = roughness * roughness;
		float newGloss = r2 / (ft * (1.0 + (1.0 - ft) / r2));
		roughness = sqrt(newGloss); // Feed this adjusted roughness into your BRDF
	}

	// Final Lighting
	float fateFactor = fastWorley3d(vec3(FragPos.xz / 50.0, time * 0.25)) * 0.5 + 0.50;
	vec3  windForce = fastCurl3d(
		vec3(FragPos.x * 0.0005 + time * 0.00125, FragPos.y * 0.001, FragPos.z * 0.0005 + time * 0.0125)
	);
	vec3 rawWindNudge = (fateFactor * windForce); // / (abs(normalize(FragPos).y - normalize(windForce).y));

	vec3  light_dir = normalize(lights[0].position - FragPos);
	float rim = max(dot(light_dir, normalize(viewPos - FragPos)), 0.0);
	// albedo += (1-dot(rawWindNudge, perturbedNorm)) * rim * albedo;
	float windDistortion = pow(
		1 -
			smoothstep(
				0,
				1,
				(max(0, dot(vec3(0, 1, 0), perturbedNorm)) * ((1 - dot(rawWindNudge, perturbedNorm)) / 2))
			),
		9.0
	);
	float plainRipple = tangentGabor(FragPos, norm, -1 * windDistortion * rawWindNudge, time, 0.5, 0.00001, 0.75) *
			0.5 +
		0.5;
	float windRipple = windDistortion * plainRipple;
	float grassFactor = smoothstep(0.25, 0.5, max(dot(albedo, COL_GRASS_LUSH), dot(albedo, COL_GRASS_DRY)));
	albedo *= mix(1, mix(1.0, 1.25, windDistortion) * mix(1.0, 1.05, windRipple), grassFactor);
	roughness *= mix(1.25, 1.0, windDistortion) * mix(1, mix(1.5, 1.0, windRipple), grassFactor);
	// perturbedNorm += rawWindNudge * mix(0.0, 1.05, plainRipple);

	vec3 lighting = apply_lighting_pbr(FragPos, perturbedNorm, albedo, roughness, metallic, 1.0).rgb;

	// ========================================================================
	// Neon 80s Synth Style (Night Theme)
	// ========================================================================
	// Synthwave grid lines
	float gridScale = 0.05; // Lines every 20 units
	vec2  gridUV = FragPos.xz * gridScale;

	// Use derivative-based anti-aliasing for the grid lines
	vec2  grid = abs(fract(gridUV - 0.5) - 0.5) / (fwidth(gridUV) * 1.5);
	float line = min(grid.x, grid.y);
	float gridLine = 1.0 - smoothstep(0.0, 1.0, line);

	// Thicker grid for glow effect
	vec2  gridGlow = abs(fract(gridUV - 0.5) - 0.5) / (fwidth(gridUV) * 8.0);
	float lineGlow = min(gridGlow.x, gridGlow.y);
	float gridGlowFactor = 1.0 - smoothstep(0.0, 1.0, lineGlow);

	vec3 cyan = vec3(0.0, 1.0, 1.0);
	vec3 magenta = vec3(1.0, 0.0, 1.0);

	// Blend albedo towards dark purple/magenta for that 80s look
	vec3 newLighting = mix(lighting, lighting * vec3(0.4, 0.1, 0.5), 0.7);

	// Add cyan grid with magenta glow
	newLighting += gridLine * cyan * 0.8;
	newLighting += gridGlowFactor * magenta * 0.4;
	vec3 gridLight = newLighting;

	// Height-based neon pulse/glow
	float heightGlow = smoothstep(0.0, 100.0 * worldScale, FragPos.y);
	newLighting += magenta * heightGlow * (0.8 + 0.2 * sin(time * 0.5));

	float nightNoise = fastWorley3d(vec3(FragPos.xy / (25 * worldScale), time * 0.08));
	float nightFade = smoothstep(fade_start - 10, fade_end, dist + nightNoise * 100.0);
	lighting = mix(mix(lighting, gridLight, smoothstep(fade_start - 150, fade_end - 20, dist)), newLighting, nightFade);

	// ========================================================================
	// Distance Fade
	// ========================================================================
	// The AtmosphereEffect will handle the scattering over the terrain.
	// We just handle the alpha fade for transition to the skybox.
	vec4 baseColor = vec4(lighting, mix(0.0, fade, step(0.01, FragPos.y)));

	// Restore deliberate cyan style for distant terrain
	FragColor = mix(vec4(0.0, 0.7, 0.7, baseColor.a) * length(baseColor), baseColor, step(1.0, fade));
}
#endif // GSHADERS_TERRAIN_FRAG
