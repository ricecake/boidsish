#version 460 core

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 Velocity;
layout(location = 2) out vec4 NormalOut;
layout(location = 3) out vec4 AlbedoOut;

in vec3       Normal;
in vec3       FragPos;
in vec4       CurPosition;
in vec4       PrevPosition;
in vec2       TexCoords;
flat in float TextureSlice;
in float      perturbFactor;
in float      vIsWater;
in float      vErosionDelta;
in float      vRidgeMap;
in float      vSubstrate;

#define USE_TERRAIN_DATA
#include "helpers/erosion.glsl"
#include "helpers/fast_noise.glsl"
#include "helpers/terrain_noise.glsl"
#include "helpers/terrain_shadows.glsl"
#include "helpers/lighting.glsl"
#include "helpers/fader.glsl"
#include "visual_effects.glsl"
#include "helpers/wind.glsl"
#include "lygia/color/palette.glsl"
#include "lygia/generative/voronoi.glsl"
#include "lygia/generative/random.glsl"

uniform bool uIsShadowPass = false;
uniform sampler2DArray uBiomeMap;
uniform float          uRawChunkSize;
uniform mat4 view;

struct GrassProperties {
    vec4  colorTop;
    vec4  colorBottom;
    float height;
    float width;
    float rigidity;
    float heightVariance;
    float widthVariance;
    float density;
    float colorVariability;
    float windInfluence;
    uint  enabled;
    float flowerRatio;
    float _pad1;
    float _pad2;
};

struct GlobalGrassProperties {
    float lengthMultiplier;
    float widthMultiplier;
    float densityMultiplier;
    float rigidityMultiplier;
    float windMultiplier;
    uint  enabled;
    float _pad0;
    float _pad1;
};

layout(std140, binding = [[GRASS_PROPS_BINDING]]) uniform GrassProps {
    GrassProperties u_grassBiomes[8];
    GlobalGrassProperties u_grassGlobal;
};

#define HEIGHT_BEACH_END (3.0 * worldScale)
#define HEIGHT_LOWLAND_END (20.0 * worldScale)
#define HEIGHT_FOREST_END (50.0 * worldScale)
#define HEIGHT_ALPINE_START (60.0 * worldScale)
#define HEIGHT_TREELINE (80.0 * worldScale)
#define HEIGHT_SNOW_START (90.0 * worldScale)
#define HEIGHT_PEAK (100.0 * worldScale)

const vec3 COL_SAND_WET = vec3(0.55, 0.45, 0.35);
const vec3 COL_SAND_DRY = vec3(0.76, 0.70, 0.55);
const vec3 COL_GRASS_LUSH = vec3(0.20, 0.45, 0.15);
const vec3 COL_GRASS_DRY = vec3(0.45, 0.50, 0.25);
const vec3 COL_FOREST = vec3(0.12, 0.28, 0.10);
const vec3 COL_ALPINE_MEADOW = vec3(0.35, 0.45, 0.25);
const vec3 COL_ROCK_BROWN = vec3(0.35, 0.30, 0.25);
const vec3 COL_ROCK_GREY = vec3(0.45, 0.45, 0.48);
const vec3 COL_ROCK_DARK = vec3(0.25, 0.23, 0.22);
const vec3 COL_SNOW_FRESH = vec3(0.95, 0.97, 1.00);
const vec3 COL_SNOW_OLD = vec3(0.85, 0.88, 0.92);

struct TerrainMaterial {
	vec3  albedo;
	float roughness;
	float metallic;
	float normalScale;
	float normalStrength;
};

float calculateValleyFactor(vec3 pos) {
	float scale = 0.02;
	float center = length(pos * scale);
	float dx = 5.0;
	float north = length((pos + vec3(0, 0, dx)) * scale);
	float south = length((pos - vec3(0, 0, dx)) * scale);
	float east = length((pos + vec3(dx, 0, 0)) * scale);
	float west = length((pos - vec3(dx, 0, 0)) * scale);
	return ((north + south + east + west) / 4.0 - center) * 10.0;
}

float calculateMoisture(float height, float valleyFactor, vec3 pos) {
	float baseMoisture = 1.0 - smoothstep(0.0, HEIGHT_PEAK, height) * 0.6;
	float valleyMoisture = clamp(-valleyFactor * 0.5, 0.0, 0.4);
	float noiseMoisture = fastSimplex3d(pos * 0.03) * 0.2;
	return clamp(baseMoisture + valleyMoisture + noiseMoisture, 0.0, 1.0);
}

/**
 * Procedural pebble/rock detail layer
 * Returns albedo multiplier and normal perturbation
 */
void getPebbleDetail(vec3 pos, vec3 norm, float scale, out float albedoMod, out vec3 normalMod) {
    vec3 p = pos * scale;
    vec3 v = voronoi(p.xz);
    float d = v.z;

    // Create pebble shapes
    float pebble = 1.0 - smoothstep(0.0, 0.5, d);
    pebble *= step(0.3, random(v.xy)); // Randomly cull some pebbles

    albedoMod = 1.0 + pebble * 0.2 * (random(v.xy) - 0.5);

    // Normal from distance gradient
    float eps = 0.1;
    float dx = voronoi(p.xz + vec2(eps, 0.0)).z - d;
    float dz = voronoi(p.xz + vec2(0.0, eps)).z - d;

    // Use a stable tangent basis
    vec3 tangent = abs(norm.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
    tangent = normalize(cross(norm, tangent));
    vec3 bitangent = cross(norm, tangent);
    normalMod = (tangent * dx + bitangent * dz) * pebble * 2.0;
}

TerrainMaterial getBiomeMaterial(float height, float moisture, float noise) {
	float realDist = distance(FragPos, viewPos);
	TerrainMaterial mat;
	mat.metallic = 0.0;
	float h = height + noise * 8.0;

	if (h < HEIGHT_BEACH_END) {
		float rockFactor = (fastWorley3d(FragPos/125.0) * 0.5 + 0.5) * ((1.0 - smoothstep(0.0, HEIGHT_BEACH_END, height)) + smoothstep(HEIGHT_TREELINE, HEIGHT_SNOW_START, height));
		float wetnessFactor = 1.0 - smoothstep(0.0, HEIGHT_BEACH_END, h);

		mat.albedo = mix(COL_SAND_DRY, COL_SAND_WET, wetnessFactor);
		mat.roughness = mix(0.9, 0.4, wetnessFactor);
		mat.normalScale = 40.0;
		mat.normalStrength = mix(0.1, 0.05, wetnessFactor);

		if (rockFactor > 0.5) {
			vec3 rockBoundary = voronoi((TexCoords + (noise * 0.05)) * int(50.0 * mix(5.0, 0.1, smoothstep(50.0, 250.0, 20.0 * int(realDist / 20.0)))));
			float rockPalette = step(0.5, random(rockBoundary.xy));
			vec3 color = palette(random(rockBoundary.xy), vec3(0.5), vec3(0.5), mix(vec3(1.0, 1.0, 0.5), vec3(1.0), rockPalette), mix(vec3(0.8, 0.9, 0.3), vec3(0.3, 0.2, 0.2), rockPalette));
			vec3 rockColor = color * ((1.0 - smoothstep(0.0, max(0.75, random(rockBoundary.xy)), rockBoundary.z)) * 0.8 + 0.2);
			mat.albedo = mix(mat.albedo, rockColor, smoothstep(0.5, 1.0, rockFactor));
			mat.roughness = mix(0.7, 0.1, wetnessFactor * smoothstep(0.01, 0.02, rockBoundary.z));
		}
		return mat;
	}

	if (h < HEIGHT_LOWLAND_END) {
		float t = smoothstep(HEIGHT_BEACH_END, HEIGHT_LOWLAND_END, h);
		vec3 grassColor = mix(COL_GRASS_LUSH, COL_GRASS_DRY, t * (1.0 - moisture));
		float sandFade = smoothstep(HEIGHT_BEACH_END, HEIGHT_BEACH_END + 5.0, h);
		mat.albedo = mix(COL_SAND_DRY, grassColor, sandFade);
		mat.roughness = mix(0.9, mix(0.7, 0.8, t * (1.0 - moisture)), sandFade);
		mat.normalScale = mix(40.0, 12.0, sandFade);
		mat.normalStrength = mix(0.1, 0.08, sandFade);
		return mat;
	}

	if (h < HEIGHT_FOREST_END) {
		float t = smoothstep(HEIGHT_LOWLAND_END, HEIGHT_FOREST_END, h);
		vec3 forestColor = mix(COL_GRASS_LUSH, COL_FOREST, moisture);
		mat.albedo = mix(forestColor, COL_GRASS_DRY, t * 0.3);
		mat.roughness = mix(0.8, 0.85, t * 0.3);
		mat.normalScale = mix(12.0, 10.0, t);
		mat.normalStrength = mix(0.08, 0.12, t);
		return mat;
	}

	if (h < HEIGHT_ALPINE_START) {
		float t = smoothstep(HEIGHT_FOREST_END, HEIGHT_ALPINE_START, h);
		mat.albedo = mix(COL_FOREST, COL_ALPINE_MEADOW, t);
		mat.roughness = 0.8;
		mat.normalScale = mix(10.0, 15.0, t);
		mat.normalStrength = mix(0.12, 0.1, t);
		return mat;
	}

	if (h < HEIGHT_TREELINE) {
		float t = smoothstep(HEIGHT_ALPINE_START, HEIGHT_TREELINE, h);
		mat.albedo = mix(COL_ALPINE_MEADOW, COL_ROCK_GREY, t * 0.4);
		mat.roughness = mix(0.8, 0.6, t * 0.4);
		mat.normalScale = mix(15.0, 4.0, t * 0.4);
		mat.normalStrength = mix(0.1, 0.2, t * 0.4);
		return mat;
	}

	if (h < HEIGHT_SNOW_START) {
		float t = smoothstep(HEIGHT_TREELINE, HEIGHT_SNOW_START, h);
		vec3 rockColor = mix(COL_ROCK_BROWN, COL_ROCK_GREY, noise * 0.5 + 0.5);
		vec3 patchColor = mix(rockColor, COL_ALPINE_MEADOW, moisture * 0.3);
		mat.albedo = mix(patchColor, COL_SNOW_OLD, t * 0.3);
		mat.roughness = mix(0.6, 0.5, t * 0.3);
		mat.normalScale = mix(4.0, 25.0, t * 0.3);
		mat.normalStrength = mix(0.2, 0.05, t * 0.3);
		return mat;
	}

	float t = smoothstep(HEIGHT_SNOW_START, HEIGHT_PEAK, h);
	vec3 snowColor = mix(COL_SNOW_OLD, COL_SNOW_FRESH, t);
	float rockShow = (1.0 - t) * 0.2 * (1.0 - moisture);
	mat.albedo = mix(snowColor, COL_ROCK_GREY, rockShow);
	mat.roughness = mix(0.5, 0.4, t);
	mat.normalScale = mix(25.0, 30.0, t);
	mat.normalStrength = mix(0.05, 0.03, t);
	return mat;
}

TerrainMaterial getCliffMaterial(float height, float noise) {
	TerrainMaterial mat;
	mat.metallic = 0.0;
	float h = height + noise * 5.0;

	if (h < HEIGHT_FOREST_END) {
		float wetnessFactor = 0.3 + noise * 0.2;
		mat.albedo = mix(COL_ROCK_BROWN, COL_ROCK_DARK, wetnessFactor);
		mat.roughness = mix(0.6, 0.3, wetnessFactor);
		mat.normalScale = 4.0;
		mat.normalStrength = 0.2;
		return mat;
	}

	if (h < HEIGHT_SNOW_START) {
		float t = smoothstep(HEIGHT_FOREST_END, HEIGHT_SNOW_START, h);
		mat.albedo = mix(COL_ROCK_BROWN, COL_ROCK_GREY, t + noise * 0.2);
		mat.roughness = 0.6;
		mat.normalScale = 3.5;
		mat.normalStrength = 0.2;
		return mat;
	}

	float snowPatch = smoothstep(HEIGHT_SNOW_START, HEIGHT_PEAK, h) * 0.4;
	vec3  highRock = mix(COL_ROCK_GREY, COL_ROCK_DARK, noise * 0.3);
	mat.albedo = mix(highRock, COL_SNOW_OLD, snowPatch);
	mat.roughness = mix(0.6, 0.5, snowPatch);
	mat.normalScale = 3.0;
	mat.normalStrength = 0.15;
	return mat;
}

void main() {
	if (uIsShadowPass) return;

	vec3  norm = normalize(Normal);
	float slope = dot(norm, vec3(0.0, 1.0, 0.0));
	float dist = length(FragPos.xz - viewPos.xz);
	float realDist = distance(FragPos, viewPos);
	float baseFreq = 0.1 / worldScale;

	if (vIsWater > 0.5) {
		vec3 surfaceColor = vec3(0.05, 0.05, 0.08);
		vec3 finalColor = applyWaterGrid(surfaceColor, FragPos, norm, dist, time);
		float primaryShadow;
		vec3 lighting = apply_lighting_pbr(FragPos, norm, finalColor, 0.05, 0.9, 1.0, primaryShadow).rgb;
		FragColor = applyStylisticFade(lighting, FragPos, dist, worldScale, time);
		NormalOut = vec4(normalize(mat3(view) * norm), primaryShadow);
		AlbedoOut = vec4(surfaceColor, 1.0);
		return;
	}

    float sharedFade = fastSimplex3d(vec3(FragPos.xz / (250.0 * worldScale), time * 0.09));
	float largeNoise = fastWarpedFbm3d(FragPos * (baseFreq * 0.1));
	float baseHeight = FragPos.y;
	float distortedHeight = baseHeight + largeNoise * 5.0 * worldScale;
	float distortedSlope = slope + largeNoise * 0.08;
	float valleyFactor = calculateValleyFactor(FragPos);
	float moisture = calculateMoisture(baseHeight, valleyFactor, FragPos);
	moisture = mix(moisture, min(moisture + 0.3, 1.0), clamp(-valleyFactor, 0.0, 1.0));

	float cliffThreshold = mix(0.4, 0.3, smoothstep(HEIGHT_SNOW_START, HEIGHT_PEAK, baseHeight));
	float cliffMask = max(smoothstep(cliffThreshold, cliffThreshold - 0.15, distortedSlope), smoothstep(0.4, 0.2, slope));
	cliffMask = clamp(cliffMask + (largeNoise - 0.5) * 0.15, 0.0, 1.0);
	cliffMask *= (1.0 - (1.0 - smoothstep(0.0, HEIGHT_BEACH_END + 2.0, baseHeight)));
	cliffMask = clamp(cliffMask + smoothstep(0.2, -0.6, vSubstrate) * 0.4, 0.0, 1.0);

	TerrainMaterial biomeMat = getBiomeMaterial(distortedHeight, moisture, largeNoise);
	TerrainMaterial cliffMat = getCliffMaterial(baseHeight, largeNoise);

	TerrainMaterial finalMaterial;
	finalMaterial.albedo = mix(biomeMat.albedo, cliffMat.albedo, cliffMask) * (1.0 + largeNoise * 0.12) * (1.0 + largeNoise * 0.15);
	finalMaterial.roughness = mix(biomeMat.roughness, cliffMat.roughness, cliffMask);
	finalMaterial.metallic = mix(biomeMat.metallic, cliffMat.metallic, cliffMask);
	finalMaterial.normalScale = mix(biomeMat.normalScale, cliffMat.normalScale, cliffMask);
	finalMaterial.normalStrength = mix(biomeMat.normalStrength, cliffMat.normalStrength, cliffMask);

    // Apply pebble detail to rocky areas and beaches
    float pebbleAlbedoMod = 1.0;
    vec3 pebbleNormalMod = vec3(0.0);
    float pebbleMask = max(cliffMask, 1.0 - smoothstep(0.0, HEIGHT_BEACH_END + 5.0, baseHeight));
    if (pebbleMask > 0.01 && realDist < 50.0) {
        getPebbleDetail(FragPos, norm, 10.0, pebbleAlbedoMod, pebbleNormalMod);
        float strength = pebbleMask * smoothstep(50.0, 30.0, realDist);
        finalMaterial.albedo *= mix(1.0, pebbleAlbedoMod, strength);
    }

	float freezingScale = 1.0 - smoothstep(255.372, 273.15, temperature);
	float globalWetness = max(wetness, freezingScale);
	finalMaterial.albedo = mix(finalMaterial.albedo, finalMaterial.albedo * 0.5, globalWetness * 0.5);
	finalMaterial.roughness = mix(finalMaterial.roughness, 0.1, globalWetness * 0.8);
	finalMaterial.albedo = mix(finalMaterial.albedo, finalMaterial.albedo * (1.0 + largeNoise * 0.2), smoothstep(0.5, 0.2, slope));
	finalMaterial.albedo = applyErosionColorMappingDefault(finalMaterial.albedo, vRidgeMap, vErosionDelta);

	float grassAO = 0.0;
	vec3 perturbedNorm = norm;
	if (u_grassGlobal.enabled != 0 && freezingScale == 0) {
		float stepDist = 50.0 * int(realDist / 50.0);
		float freqScale = mix(5.0, mix(1.0, 0.25, smoothstep(150.0, 160.0, stepDist + 50.0 * largeNoise)), smoothstep(45.0, 50.0, stepDist));
		float blueNoise = fastBlueNoise(FragPos.xz * (baseFreq * 0.05 * freqScale), 0) * 0.5 + 0.5;
		float blueNoiseA = fastBlueNoise(FragPos.xz * (baseFreq * 0.1 * freqScale), 1) * 0.5 + 0.5;

		vec2 biomeUV = (TexCoords * uRawChunkSize + 0.5) / (uRawChunkSize + 1.0);
		vec2 biomeData = texture(uBiomeMap, vec3(biomeUV, TextureSlice)).rg;
		int idxA = int(biomeData.r * 255.0 + 0.5);
		int idxB = min(idxA + 1, 7);
		float t = biomeData.g;

		float interpolatedDensity = mix(u_grassBiomes[idxA].density * float(u_grassBiomes[idxA].enabled), u_grassBiomes[idxB].density * float(u_grassBiomes[idxB].enabled), t) * u_grassGlobal.densityMultiplier;
		vec3 grassColor = mix(u_grassBiomes[idxA].colorBottom.rgb, u_grassBiomes[idxB].colorBottom.rgb, smoothstep(0.0, blueNoiseA, t));
		float rigidity = clamp(mix(u_grassBiomes[idxA].rigidity, u_grassBiomes[idxB].rigidity, step(blueNoise, t)) * u_grassGlobal.rigidityMultiplier, 0.0, 1.0);

		float grassMask = smoothstep(0.7, 0.8, norm.y) * clamp(interpolatedDensity, 0.0, 1.0);
		grassAO = grassMask * 0.75;
		perturbedNorm = normalize(mix(norm, vec3(0.0, 1.0, 0.0), interpolatedDensity * smoothstep(200.0, 350.0, dist)));

		float windAtPos = FragPos.x * sin(sharedFade) + FragPos.z * cos(sharedFade);
		float effectiveWindStrength = max(0.0, length(windAtPos) - rigidity * 2.0);
		finalMaterial.albedo = mix(finalMaterial.albedo, mix(grassColor, grassColor * 1.25 + vec3(0.05, 0.05, 0.0), smoothstep(0.0, blueNoise, effectiveWindStrength)), smoothstep(0.0, blueNoise, grassMask));
		finalMaterial.albedo *= mix(1.0, pow(fastRidge3d(FragPos * 0.01 * freqScale) * 0.5 + 0.5, 2.0), (1.0 - smoothstep(0.0, 150.0, dist)));
		finalMaterial.roughness = clamp(finalMaterial.roughness, 0.0, 1.0);
	}

    // Apply pebble normal perturbation
    if (pebbleMask > 0.01 && realDist < 50.0) {
        perturbedNorm = normalize(perturbedNorm + pebbleNormalMod * pebbleMask * smoothstep(50.0, 30.0, realDist));
    }

	float roughness = finalMaterial.roughness;
	if (perturbFactor >= 0.1 && finalMaterial.normalStrength > 0.0 && (dist + 50.0 * largeNoise) < 200.0) {
		float roughnessStrength = smoothstep(0.1, 1.0, perturbFactor) * finalMaterial.normalStrength;
		float roughnessScale = finalMaterial.normalScale * 0.05;
		vec3  scaledFragPos = FragPos / worldScale;
		float eps = 0.015;
		float n, nx, nz;
		if (freezingScale < 0.5) {
			n = fastRidge3d(0.1 * scaledFragPos * roughnessScale);
			nx = fastRidge3d(0.1 * (scaledFragPos + vec3(eps, 0.0, 0.0)) * roughnessScale);
			nz = fastRidge3d(0.1 * (scaledFragPos + vec3(0.0, 0.0, eps)) * roughnessScale);
		} else {
			n = fastWarpedFbm3d(0.1 * scaledFragPos * roughnessScale);
			nx = fastWarpedFbm3d(0.1 * (scaledFragPos + vec3(eps, 0.0, 0.0)) * roughnessScale);
			nz = fastWarpedFbm3d(0.1 * (scaledFragPos + vec3(0.0, 0.0, eps)) * roughnessScale);
		}
		vec3 v = abs(perturbedNorm.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
		vec3 tangent = normalize(cross(perturbedNorm, v));
		vec3 bitangent = cross(tangent, perturbedNorm);
		vec3 perturbation = (tangent * (n - nx) + bitangent * (n - nz)) * (roughnessStrength / eps);
		perturbedNorm = normalize(perturbedNorm + perturbation);
		roughness = sqrt(clamp(roughness * roughness + dot(perturbation, perturbation) * 0.25, 0.0, 1.0));
	}

	vec3 albedo = freezingScale > 0 ? mix(finalMaterial.albedo, vec3(1.1, 1.1, 1.1 + 0.1 * grassAO), freezingScale) : finalMaterial.albedo;
	float metallic = freezingScale > 0 ? mix(finalMaterial.metallic, 1.0, freezingScale) : finalMaterial.metallic;

	float primaryShadow;
	vec3 lighting = apply_lighting_pbr(FragPos, perturbedNorm, albedo, roughness, metallic, 1.0 - grassAO, primaryShadow).rgb;

	lighting.b *= 1.0 + (0.2 * freezingScale * (1.0 - primaryShadow));

	FragColor = applyStylisticFade(lighting, FragPos, dist, worldScale, time);
	NormalOut = vec4(normalize(mat3(view) * perturbedNorm), primaryShadow);
	AlbedoOut = vec4(albedo, 1.0);

	vec2 a = (CurPosition.xy / CurPosition.w) * 0.5 + 0.5;
	vec2 b = (PrevPosition.xy / PrevPosition.w) * 0.5 + 0.5;
	Velocity = vec4(a - b, roughness, metallic);
}
