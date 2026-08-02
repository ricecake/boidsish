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
in float      tessFactor;
in float      vIsWater;
in float      vErosionDelta;
in float      vRidgeMap;
in float      vSubstrate;
in float      vIsBaked;

#define USE_TERRAIN_DATA
#include "helpers/erosion.glsl"
#include "helpers/fast_noise.glsl"
#include "helpers/terrain_noise.glsl"
#include "helpers/terrain_shadows.glsl"
#include "helpers/lighting.glsl"
#include "visual_effects.glsl"
#include "helpers/wind.glsl"
#include "lygia/color/palette.glsl"
#include "lygia/generative/voronoi.glsl"
#include "lygia/generative/random.glsl"

uniform bool uIsShadowPass = false;

// Biome texture array: RG8 - R=low_idx, G=t
uniform sampler2DArray uBiomeMap;
// Baked displacement: RGB=displacement.xyz, A=biome_override
uniform sampler2DArray u_displacementArray;
uniform float          uRawChunkSize;

// 3D terrain color blend texture: X=height, Y=moisture, Z=roughness
layout(binding = [[TERRAIN_COLOR_BLEND_BINDING]]) uniform sampler3D u_terrainColorBlend;

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
    float lodScaleFactor;
    float lodBaseRange;
    float baseScale;
    float _pad0;
    float _pad1;
    float _pad2;
};

layout(std140, binding = [[GRASS_PROPS_BINDING]]) uniform GrassProps {
    GrassProperties u_grassBiomes[8];
    GlobalGrassProperties u_grassGlobal;
};

struct BiomeProperties {
	vec4 albedo_roughness; // rgb = albedo, w = roughness
	vec4 params;           // x = metallic, y = detailStrength, z = detailScale, w = unused
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

struct TerrainContext {
	// Topography
	float rawHeight;
	float perturbedHeight;
	float slope;
	float valleyFactor;

	// Climate & Weather
	float moisture;
	float freezingScale;
	float globalWetness;

	// Geological / Erosion State
	float substrate;
	float creaseMask;
	float ridgeMask;

	// Derived Masks
	float cliffMask;

	// View/Distance Info
	float dist;
	float realDist;

	// Consolidated Biome Info
	int   biomeIdxA;
	int   biomeIdxB;
	float biomeT;
};

float curvature (vec3 pos, vec3 norm) {
    vec3 n = normalize(norm);

    vec3 dx = dFdx(n);
    vec3 dy = dFdy(n);

    vec3 xneg = n - dx;
    vec3 xpos = n + dx;
    vec3 yneg = n - dy;
    vec3 ypos = n + dy;

  float depth = length(pos);

    float curvature = (cross(xneg, xpos).y - cross(yneg, ypos).x) * 4 / depth;

	return curvature;

    // if (curvature > 0.0) {
    //     // Convex surface (ridge / mountain)
    //     fragColor = vec4(vec3(0.0, 1.0, 0.0) * abs(curvature) * 10.0, 1.0);
    // } else {
    //     // Concave surface (valley / cavity)
    //     fragColor = vec4(vec3(1.0, 0.0, 0.0) * abs(curvature) * 10.0, 1.0);
    // }
}

/**
 * Calculate valley/ridge factor using noise-based curvature approximation.
 * Valleys tend to accumulate moisture and be more lush.
 * Returns: negative = valley (concave), positive = ridge (convex)
 */
float calculateValleyFactor(vec3 pos, vec3 norm) {
	float up = dot(Normal, vec3(0,1,0));
	float range = smoothstep(0.7, 0.75, up) - smoothstep(0.8, 0.9, up);
	float curve = curvature(FragPos, Normal);
	float vallyIshNess = smoothstep(0.0, 0.02, -curve) * range;

	return vallyIshNess;
	// float normalChange = length(fwidth(normal));
	// float pixelSize = length(fwidth(position));
	// float curvature = normalChange / max(pixelSize, 0.0001);
	// Sample noise at different scales to approximate local curvature
	// float scale = 0.02;
	// float center = length(pos * scale);

	// // Sample neighbors
	// float dx = 5.0;
	// float north = length((pos + vec3(0, 0, dx)) * scale);
	// float south = length((pos - vec3(0, 0, dx)) * scale);
	// float east = length((pos + vec3(dx, 0, 0)) * scale);
	// float west = length((pos - vec3(dx, 0, 0)) * scale);

	// // Laplacian approximation - negative means we're lower than surroundings (valley)
	// float laplacian = (north + south + east + west) / 4.0 - center;

	// return laplacian * 10.0; // Scale for usability
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
 * Extract the terrain context to drive specific feature selections.
 * Consolidates duplicate texture reads.
 */
TerrainContext extractTerrainContext(
	vec3 fragPos, vec3 normal, float largeNoise,
	float vRidgeMap, float vSubstrate,
	float temperature, float wetness,
	float dist, float realDist
) {
	TerrainContext ctx;

	// Topography
	ctx.rawHeight = fragPos.y;
	ctx.perturbedHeight = ctx.rawHeight + largeNoise * 5.0 * worldScale;

	ctx.slope = dot(normal, vec3(0.0, 1.0, 0.0));
	float distortedSlope = ctx.slope + largeNoise * 0.08;

	ctx.valleyFactor = calculateValleyFactor(fragPos, normal);

	// Climate
	ctx.moisture = calculateMoisture(ctx.rawHeight, ctx.valleyFactor, fragPos);
	float valleyLushness = clamp(-ctx.valleyFactor, 0.0, 1.0);
	ctx.moisture = mix(ctx.moisture, min(ctx.moisture + 0.3, 1.0), valleyLushness);

	ctx.freezingScale = 1.0 - smoothstep(255.372, 273.15, temperature);
	ctx.globalWetness = max(wetness, ctx.freezingScale);

	// Geological
	ctx.substrate = vSubstrate;
	ctx.creaseMask = 1.0 - smoothstep(-0.8, 0.2, vRidgeMap);
	ctx.ridgeMask = smoothstep(0.2, 0.8, vRidgeMap);

	// Derived Cliff Mask
	float cliffThreshold = abs(largeNoise * 0.1) + mix(0.5, 0.4, smoothstep(HEIGHT_SNOW_START, HEIGHT_PEAK, ctx.rawHeight));
	float rawCliff = 1.0 - smoothstep(cliffThreshold - 0.15, cliffThreshold, distortedSlope);
	float verticalMask = 1.0 - smoothstep(0.2, 0.4, ctx.slope);

	ctx.cliffMask = max(rawCliff, verticalMask);

	// Prevent cliffs on beaches
	float beachMask = 1.0 - smoothstep(0.0, HEIGHT_BEACH_END + 2.0, ctx.rawHeight);
	ctx.cliffMask *= (1.0 - beachMask);

	// Substrate modifier
	float substrateCliffFactor = smoothstep(-0.6, 0.2, ctx.substrate);
	ctx.cliffMask = clamp(ctx.cliffMask + substrateCliffFactor * 0.4, 0.0, 1.0);

	// View & distance
	ctx.dist = dist;
	ctx.realDist = realDist;

	// Consolidated Biome Texture Reads
	vec2 biomeUV = (TexCoords * uRawChunkSize + 0.5) / (uRawChunkSize + 1.0);
	vec4 biomeData = texture(uBiomeMap, vec3(biomeUV, TextureSlice));

	// Baked biome override
	vec4 bakedDisp = texture(u_displacementArray, vec3(biomeUV, TextureSlice));
	if (vIsBaked > 0.5) {
		if (bakedDisp.a > 0.0) {
			biomeData.x = bakedDisp.a;
			biomeData.y = 0.0; // Full override
		}
	} else {
		// FALLBACK: Recalculate outcrop mask for unbaked chunks
		vec2 outcropWID = fastWorley3dID(fragPos * 0.05);
		float outcropMask = smoothstep(0.7, 0.9, outcropWID.x);
		if (outcropMask > 0.0) {
			biomeData.x = 6.0 / 255.0; // GreyRock index
			biomeData.y = 0.0;
		}
	}

	ctx.biomeIdxA = int(biomeData.r * 255.0 + 0.5);
	ctx.biomeIdxB = min(ctx.biomeIdxA + 1, 7);
	ctx.biomeT = biomeData.g;

	return ctx;
}

/**
 * Get rock texture properties.
 */
TerrainMaterial getRockTexture(TerrainContext ctx, vec3 baseColor, float noise) {
	TerrainMaterial mat;
	float h = ctx.rawHeight + noise * 8.0;

	vec2 rockFactors = (fastWorley3dID(FragPos / 125.0));
	float rockFactor = clamp(0.25 + abs(rockFactors.x), 0.0, 1.0);
	float wetness = 1.0 - smoothstep(0.0, HEIGHT_BEACH_END, h) + ctx.moisture;

	mat.albedo = mix(baseColor, mix(COL_ROCK_GREY, COL_ROCK_DARK, noise * 0.3), rockFactor);
	mat.roughness = mix(0.9, 0.4, wetness);
	mat.normalScale = 40.0;
	mat.normalStrength = mix(0.1, 0.05, wetness);

	if (rockFactor > 0.0) {
		vec2 rockBoundary = fastWorley3dID(FragPos * 0.2);

		float rockPalette = clamp(rockFactors.y, 0.0, 1.0);
		vec3 color = palette(
			(rockBoundary.y),
			vec3(0.5, 0.5, 0.5), vec3(0.5, 0.5, 0.5),
			mix(vec3(1.0, 0.7, 0.40), vec3(1.0, 1.0, 0.50), rockPalette),
			mix(vec3(0.0, 0.15, 0.20), vec3(0.80, 0.90, 0.30), rockPalette)
		);

		vec3 rockColor = color * ((1.0 - smoothstep(0.0, max(0.75, (rockBoundary.y)), rockBoundary.x)) * 0.8 + 0.2);

		float veinMask = smoothstep(0.7, 0.75, pow(fastRidge3d(FragPos * 0.05), 2.0));
		rockColor = mix(rockColor, vec3(0.8, 0.9, 1.0), veinMask);

		mat.albedo = mix(mat.albedo, rockColor, smoothstep(0.0, 0.75, rockFactor));
		mat.roughness = mix(0.7, 0.1, max(wetness * smoothstep(0.01, 0.02, rockBoundary.x), veinMask));
		mat.normalScale = 40.0;
		mat.normalStrength = mix(0.1, 0.05, wetness);
	}
	return mat;
}

/**
 * Get the base biome material based on height and moisture.
 */
TerrainMaterial getBiomeMaterial(TerrainContext ctx, float noise) {
	TerrainMaterial mat;
	mat.metallic = 0.0;
	float h = ctx.perturbedHeight;

	// Beach zone (0 - 3)
	if (h < HEIGHT_BEACH_END) {
		vec2 rockFactors = (fastWorley3dID(FragPos / 125.0) * 0.5 + 0.5) * ((1.0 - smoothstep(0.0, HEIGHT_BEACH_END, ctx.rawHeight)) + smoothstep(HEIGHT_TREELINE, HEIGHT_SNOW_START, ctx.rawHeight));
		float rockFactor = rockFactors.x;
		float wetness = 1.0 - smoothstep(0.0, HEIGHT_BEACH_END, h) + ctx.moisture;

		mat.albedo = texture(u_terrainColorBlend, vec3(h, ctx.moisture, wetness)).rgb;
		mat.roughness = mix(0.9, 0.4, wetness);
		mat.normalScale = 40.0;
		mat.normalStrength = mix(0.1, 0.05, wetness);

		if (rockFactor > 0.5) {
			vec3 rockBoundary = voronoi((TexCoords + (noise * 0.05)) * int(50.0 * mix(5.0, 0.1, smoothstep(50.0, 250.0, 20.0 * int(ctx.realDist / 20.0)))));

			float rockPalette = rockFactors.y;
			vec3 color = palette(
				random(rockBoundary.xy),
				vec3(0.5, 0.5, 0.5), vec3(0.5, 0.5, 0.5),
				mix(vec3(1.0, 1.0, 0.50), vec3(1.0, 1.0, 1.0), rockPalette),
				mix(vec3(0.80, 0.90, 0.30), vec3(0.30, 0.20, 0.20), rockPalette)
			);

			vec3 rockColor = color * ((1.0 - smoothstep(0.0, max(0.75, random(rockBoundary.xy)), rockBoundary.z)) * 0.8 + 0.2);

			mat.albedo = mix(mat.albedo, rockColor, smoothstep(0.5, 1.0, rockFactor));
			mat.roughness = mix(0.7, 0.1, wetness * smoothstep(0.01, 0.02, rockBoundary.z));
			mat.normalScale = 40.0;
			mat.normalStrength = mix(0.1, 0.05, wetness);
		}
		return mat;
	}

	// Lowland zone (3 - 25): grass/meadow, lusher in valleys
	if (h < HEIGHT_LOWLAND_END) {
		float t = smoothstep(HEIGHT_BEACH_END, HEIGHT_LOWLAND_END, h);
		vec3  grassColor = mix(COL_GRASS_LUSH, COL_GRASS_DRY, t * (1.0 - ctx.moisture));
		float grassRoughness = mix(0.7, 0.8, t * (1.0 - ctx.moisture));
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
		vec3 forestColor = mix(COL_GRASS_LUSH, COL_FOREST, ctx.moisture);
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
		vec3 patchColor = mix(rockColor, COL_ALPINE_MEADOW, ctx.moisture * 0.3);
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
	float rockShow = (1.0 - t) * 0.2 * (1.0 - ctx.moisture);
	mat.albedo = mix(snowColor, COL_ROCK_GREY, rockShow);
	mat.roughness = mix(0.5, 0.4, t);
	mat.normalScale = mix(25.0, 30.0, t);
	mat.normalStrength = mix(0.05, 0.03, t);
	return mat;
}

/**
 * Calculate cliff/steep surface material properties.
 */
TerrainMaterial getCliffMaterial(TerrainContext ctx, float noise) {
	TerrainMaterial mat;
	float h = ctx.rawHeight + noise * 5.0;

	// Low altitude cliffs: brown/dark rock (often wet)
	if (h < HEIGHT_FOREST_END) {
		float wetness = 0.3 + noise * 0.2;
		vec3 baseColor = mix(COL_ROCK_BROWN, COL_ROCK_DARK, wetness);
		mat = getRockTexture(ctx, baseColor, noise);
		mat.metallic = 0.0;
		return mat;
	}

	// Mid altitude: mixed brown/grey
	if (h < HEIGHT_SNOW_START) {
		float t = smoothstep(HEIGHT_FOREST_END, HEIGHT_SNOW_START, h);
		vec3 baseColor = mix(COL_ROCK_BROWN, COL_ROCK_GREY, t + noise * 0.2);
		mat = getRockTexture(ctx, baseColor, noise);
		mat.metallic = 0.0;
		return mat;
	}

	// High altitude cliffs: grey rock with snow patches
	float snowPatch = smoothstep(HEIGHT_SNOW_START, HEIGHT_PEAK, h) * 0.4;
	vec3  highRock = mix(COL_ROCK_GREY, COL_ROCK_DARK, noise * 0.3);
	vec3 baseColor = mix(highRock, COL_SNOW_OLD, snowPatch);
	mat = getRockTexture(ctx, baseColor, noise);
	mat.metallic = 0.0;
	return mat;
}

/**
 * Process water layers (e.g. wet surfaces, refractions).
 */
void processWaterLayer(vec3 norm, float dist, float fade) {
	float grid_spacing = 1.0;
	float rippleHeight = FragPos.y;

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

	float shimmer = 1.0 + rippleHeight * 2.0;
	float grid_intensity = max(C_minor, C_major * 1.5) * 0.6 * shimmer;
	vec3  grid_color = vec3(0.0, 0.8, 0.8) * grid_intensity;

	vec3 surfaceColor = vec3(0.05, 0.05, 0.08);

	float primaryShadow;
	vec3 lighting = apply_lighting_pbr(FragPos, norm, surfaceColor, 0.05, 0.9, 1.0, primaryShadow).rgb;
	vec3 final_color = lighting + grid_color;

	vec4 baseColor = vec4(final_color, fade);
	FragColor = mix(vec4(0.0, 0.7, 0.7, baseColor.a) * length(baseColor), baseColor, step(1.0, fade));

	NormalOut = vec4(normalize(mat3(view) * norm), primaryShadow);
	AlbedoOut = vec4(surfaceColor, 1.0);
	return;
}

/**
 * Calculate the initial material blend of biomes and cliffs.
 */
TerrainMaterial calculateMaterial(TerrainContext ctx, float largeNoise) {
	TerrainMaterial finalMaterial;

	TerrainMaterial biomeMat = getBiomeMaterial(ctx, largeNoise);
	TerrainMaterial cliffMat = getCliffMaterial(ctx, largeNoise);

	// Blend biome with cliff material
	finalMaterial.albedo = mix(biomeMat.albedo, cliffMat.albedo, ctx.cliffMask);
	finalMaterial.roughness = mix(biomeMat.roughness, cliffMat.roughness, ctx.cliffMask);
	finalMaterial.metallic = mix(biomeMat.metallic, cliffMat.metallic, ctx.cliffMask);
	finalMaterial.normalScale = mix(biomeMat.normalScale, cliffMat.normalScale, ctx.cliffMask);
	finalMaterial.normalStrength = mix(biomeMat.normalStrength, cliffMat.normalStrength, ctx.cliffMask);

	// Large-scale macro color shifts
	finalMaterial.albedo *= (1.0 + largeNoise * 0.12);
	return finalMaterial;
}

/**
 * Redone grass styling helper driven by TerrainContext.
 */
TerrainMaterial applyGrassStyling(
	TerrainContext ctx,
	TerrainMaterial mat,
	float blueNoise,
	float blueNoiseA,
	float n_fade,
	float freqScale,
	out float grassAO,
	inout vec3 perturbedNorm
) {
	grassAO = 0.0;
	if (u_grassGlobal.enabled != 0 && ctx.freezingScale == 0.0) {
		float densityA = u_grassBiomes[ctx.biomeIdxA].density * float(u_grassBiomes[ctx.biomeIdxA].enabled);
		float densityB = u_grassBiomes[ctx.biomeIdxB].density * float(u_grassBiomes[ctx.biomeIdxB].enabled);
		float interpolatedDensity = mix(densityA, densityB, ctx.biomeT) * u_grassGlobal.densityMultiplier;

		vec3 colorA = u_grassBiomes[ctx.biomeIdxA].colorBottom.rgb;
		vec3 colorB = u_grassBiomes[ctx.biomeIdxB].colorBottom.rgb;
		vec3 grassColor = mix(colorA, colorB, smoothstep(0.0, blueNoiseA, ctx.biomeT));

		float rigidA = u_grassBiomes[ctx.biomeIdxA].rigidity;
		float rigidB = u_grassBiomes[ctx.biomeIdxB].rigidity;
		float rigidity = clamp(mix(rigidA, rigidB, step(blueNoise, ctx.biomeT)) * u_grassGlobal.rigidityMultiplier, 0.0, 1.0);

		// Apply effect only on relatively flat surfaces where grass would grow
		float grassMask = smoothstep(0.7, 0.8, perturbedNorm.y) * clamp(interpolatedDensity, 0.0, 1.0);

		// AO baseline shift - darken dense grass areas
		grassAO = grassMask * 0.75;

		float distanceFactor = smoothstep(200.0, 350.0, ctx.dist);

		perturbedNorm = mix(perturbedNorm, vec3(0.0, 1.0, 0.0), interpolatedDensity * distanceFactor);
		perturbedNorm = normalize(perturbedNorm);

		float windAtPos = FragPos.x * sin(n_fade) + FragPos.z * cos(n_fade);
		float windThreshold = rigidity * 2.0;
		float effectiveWindStrength = max(0.0, length(windAtPos) - windThreshold);

		vec3 undersideColor = grassColor * 1.25 + vec3(0.05, 0.05, 0.0);
		vec3 dynamicGrassColor = mix(grassColor, undersideColor, blueNoise);

		mat.albedo = mix(mat.albedo, dynamicGrassColor, smoothstep(0.0, blueNoise, grassMask));

		float floorTexture = pow(fastRidge3d(FragPos * 0.01 * freqScale) * 0.5 + 0.5, 2.0);

		floorTexture = mix(1.0, floorTexture, (1.0 - smoothstep(0.0, 150.0, ctx.dist)));
		float albedoMultiplier = floorTexture;

		mat.albedo *= albedoMultiplier;

		mat.roughness = mix(mat.roughness, clamp(mat.roughness, 0.0, 1.0), distanceFactor);
	}
	return mat;
}

/**
 * Apply normal detail perturbation helper driven by TerrainContext.
 */
void applyDetailNormalPerturbation(
	TerrainContext ctx,
	float perturbFactor,
	float normalStrength,
	float normalScale,
	float waterEffect,
	float largeNoise,
	inout vec3 perturbedNorm,
	inout float roughness
) {
	if (perturbFactor >= 0.1 && normalStrength > 0.0 && (ctx.dist + 50.0 * largeNoise) < 200.0 && waterEffect == 0.0) {
		float roughnessStrength = smoothstep(0.1, 1.0, perturbFactor) * normalStrength;
		float roughnessScale = normalScale * 0.05;
		vec3  scaledFragPos = FragPos / worldScale;

		float noiseTypeA = u_biomes[ctx.biomeIdxA].params.w;
		float noiseTypeB = u_biomes[ctx.biomeIdxB].params.w;
		float noiseType = mix(noiseTypeA, noiseTypeB, ctx.biomeT);

		// Use finite difference to approximate the gradient of the noise field
		float eps = 0.15;
		float n, nx, nz;

		if (ctx.freezingScale < 0.5) {
			n = fastRidge3d(0.1 * scaledFragPos * roughnessScale);
			nx = fastRidge3d(0.1 * (scaledFragPos + vec3(eps, 0.0, 0.0)) * roughnessScale);
			nz = fastRidge3d(0.1 * (scaledFragPos + vec3(0.0, 0.0, eps)) * roughnessScale);
		} else {
			n = fastWarpedFbm3d(0.1 * scaledFragPos * roughnessScale);
			nx = fastWarpedFbm3d(0.1 * (scaledFragPos + vec3(eps, 0.0, 0.0)) * roughnessScale);
			nz = fastWarpedFbm3d(0.1 * (scaledFragPos + vec3(0.0, 0.0, eps)) * roughnessScale);
		}

		// Compute local tangent space to orient the perturbation.
		vec3 v = abs(perturbedNorm.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
		vec3 tangent = normalize(cross(perturbedNorm, v));
		vec3 bitangent = cross(tangent, perturbedNorm);

		// Apply perturbation based on noise gradient
		vec3 perturbation = (tangent * (n - nx) + bitangent * (n - nz)) * (roughnessStrength / eps);
		perturbedNorm = normalize(perturbedNorm + perturbation);

		// Toksvig-like Adjustment: Increase roughness based on normal variance
		float variance = dot(perturbation, perturbation);
		roughness = sqrt(clamp(roughness * roughness + variance * 0.25, 0.0, 1.0));
	}
}

TerrainMaterial generateMaterial(TerrainContext ctx, float noise) {
	TerrainMaterial mat = TerrainMaterial(vec3(0,0.0,0), 0.0, 0.0, 1.0, 1.0);

	mat.albedo = texture(u_terrainColorBlend, vec3(ctx.perturbedHeight, ctx.moisture, ctx.slope)).rgb;

	return mat;
}

void main() {
	if (uIsShadowPass) {
		// Output only depth (handled by hardware)
		return;
	}

	vec3  norm = normalize(Normal);
	float slope = dot(norm, vec3(0.0, 1.0, 0.0));

	float dist = length(FragPos.xz - viewPos.xz);
	float realDist = distance(FragPos, viewPos);

	float baseFreq = 0.1 / worldScale;
	float stepDist = 50.0 * int(realDist / 50.0);
	float freqScale = mix(1.0, 0.25, smoothstep(150.0, 160.0, stepDist + 50.0));
	freqScale = mix(5.0, freqScale, smoothstep(45.0, 50.0, stepDist));

	// ========================================================================
	// Noise Generation
	// ========================================================================
	float n_fade = fastSimplex3d(vec3(FragPos.xz / (250.0 * worldScale), time * 0.09));
	float largeNoise = fastWarpedFbm3d(FragPos * (baseFreq * 0.1));
	float blueNoise = fastBlueNoise(FragPos.xz * (baseFreq * 0.05 * freqScale), 0) * 0.5 + 0.5;
	float blueNoiseA = fastBlueNoise(FragPos.xz * (baseFreq * 0.1 * freqScale), 1) * 0.5 + 0.5;

	float fade_start = 560.0 * worldScale;
	float fade_end = 570.0 * worldScale;
	float fade = 1.0 - smoothstep(fade_start, fade_end, dist + n_fade * 40.0);

	if (fade < 0.2) {
		discard;
	}

	if (vIsWater > 0.5) {
		processWaterLayer(norm, dist, fade);
		return;
	}

	// ========================================================================
	// Context Extraction & Material Setup
	// ========================================================================
	TerrainContext ctx = extractTerrainContext(
		FragPos, norm, largeNoise,
		vRidgeMap, vSubstrate,
		temperature, wetness,
		dist, realDist
	);

	// TerrainMaterial finalMaterial = calculateMaterial(ctx, largeNoise);
	// TerrainMaterial getBiomeMaterial(TerrainContext ctx, float noise) {
	TerrainMaterial finalMaterial = generateMaterial(ctx, largeNoise);

	// Apply global wetness from precipitation
	finalMaterial.albedo = mix(finalMaterial.albedo, finalMaterial.albedo * 0.5, ctx.globalWetness * 0.5);
	finalMaterial.roughness = mix(finalMaterial.roughness, 0.1, ctx.globalWetness * 0.8);

	// ========================================================================
	// Running Water Effect
	// ========================================================================
	float waterEffect = 0.0;
	if (wetness > 0.6 && ctx.freezingScale < 0.1) {
		float rockSurface = 1.0 - smoothstep(0.2, 0.5, ctx.slope);
		rockSurface = max(rockSurface, smoothstep(0.2, -0.6, ctx.substrate));
		float waterFlowMask = rockSurface * smoothstep(0.6, 0.9, wetness);

		if (waterFlowMask > 0.01) {
			vec3 surfaceDown = vec3(0.0, -1.0, 0.0) - dot(vec3(0.0, -1.0, 0.0), norm) * norm;
			vec3 flowDir = normalize(surfaceDown + vec3(0.00001, 0.0, 0.0));
			float flowSpeed = 2.0;
			vec3 p_flow = (FragPos + -flowDir * time * flowSpeed) * 1.5;
			vec3 flowNoise = fastCurl3d(p_flow * 0.08 * mix(1.0, 0.1, smoothstep(50.0, 55.0, ctx.realDist)));

			float streaks = smoothstep(0.3, 0.8, abs(flowNoise.x));
			streaks *= smoothstep(0.4, 0.6, fract(flowNoise.y * 0.5 + time * 0.8));

			waterEffect = waterFlowMask * streaks;
			finalMaterial.albedo = mix(finalMaterial.albedo, finalMaterial.albedo * 0.5, waterEffect * 0.5);
			finalMaterial.roughness = mix(finalMaterial.roughness, 0.00, waterEffect);
			finalMaterial.metallic = mix(finalMaterial.metallic, 0.1, waterEffect);

			if (waterEffect > 0.05) {
				vec3 flowNorm = normalize(flowNoise * 2.0 - 1.0);
				norm = normalize(mix(norm, flowNorm, waterEffect * 0.8));
			}
		}
	}

	// // Extra variety for rocky/steep areas
	// float rockyVar = largeNoise;
	// float rockyMask = smoothstep(0.5, 0.2, ctx.slope);
	// finalMaterial.albedo = mix(finalMaterial.albedo, finalMaterial.albedo * (1.0 + rockyVar * 0.2), rockyMask);

	// ========================================================================
	// Advanced Erosion Filter Coloration
	// ========================================================================
	// finalMaterial.albedo = applyErosionColorMappingDefault(finalMaterial.albedo, vRidgeMap, vErosionDelta);

	// ========================================================================
	// Grass-based Styling
	// ========================================================================
	float grassAO = 0.0;
	vec3 perturbedNorm = norm;
	finalMaterial = applyGrassStyling(
		ctx, finalMaterial,
		blueNoise, blueNoiseA,
		n_fade, freqScale,
		grassAO, perturbedNorm
	);

	vec3  albedo = finalMaterial.albedo;
	float roughness = finalMaterial.roughness;
	float metallic = finalMaterial.metallic;
	float normalStrength = finalMaterial.normalStrength;
	float normalScale = finalMaterial.normalScale;

	// ========================================================================
	// Normal Detail Perturbation
	// ========================================================================
	applyDetailNormalPerturbation(
		ctx, perturbFactor, normalStrength, normalScale,
		waterEffect, largeNoise, perturbedNorm, roughness
	);

	if (ctx.freezingScale > 0.0) {
		vec3 snowColor = vec3(0.9, 0.95, 1.0 + 0.01 * grassAO);
		albedo = mix(albedo, snowColor, ctx.freezingScale);
		roughness = mix(roughness, 0.85, ctx.freezingScale);
		metallic = mix(metallic, 0.0, ctx.freezingScale);
	}

	float primaryShadow;
	vec3 lighting = apply_lighting_pbr(FragPos, perturbedNorm, albedo, roughness, metallic, 1.0 - grassAO, primaryShadow).rgb;
	lighting.b *= 1.0 + (0.2 * ctx.freezingScale * (1.0 - primaryShadow));

	// ========================================================================
	// Neon 80s Synth Style (Night Theme)
	// ========================================================================
	float gridScale = 0.05;
	vec2  gridUV = FragPos.xz * gridScale;

	vec2  grid = abs(fract(gridUV - 0.5) - 0.5) / (fwidth(gridUV) * 1.5);
	float line = min(grid.x, grid.y);
	float gridLine = 1.0 - smoothstep(0.0, 1.0, line);

	vec2  gridGlow = abs(fract(gridUV - 0.5) - 0.5) / (fwidth(gridUV) * 8.0);
	float lineGlow = min(gridGlow.x, gridGlow.y);
	float gridGlowFactor = 1.0 - smoothstep(0.0, 1.0, lineGlow);

	vec3 cyan = vec3(0.0, 1.0, 1.0);
	vec3 magenta = vec3(1.0, 0.0, 1.0);

	vec3 newLighting = mix(lighting, lighting * vec3(0.4, 0.1, 0.5), 0.7);

	newLighting += gridLine * cyan * 0.8;
	newLighting += gridGlowFactor * magenta * 0.4;
	vec3 gridLight = newLighting;

	float heightGlow = smoothstep(0.0, 100.0 * worldScale, FragPos.y);
	newLighting += magenta * heightGlow * (0.8 + 0.2 * sin(time * 0.5));

	float nightNoise = fastWorley3d(vec3(FragPos.xy / (25.0 * worldScale), time * 0.08));
	float nightFade = smoothstep(fade_start - 10.0, fade_end, dist + nightNoise * 100.0);
	lighting = mix(mix(lighting, gridLight, smoothstep(fade_start - 150.0, fade_end - 20.0, dist)), newLighting, nightFade);

	// ========================================================================
	// Distance Fade
	// ========================================================================
	vec4 baseColor = vec4(lighting, mix(0.0, fade, step(0.01, FragPos.y)));

	FragColor = mix(vec4(0.0, 0.7, 0.7, baseColor.a) * length(baseColor), baseColor, step(1.0, fade));
	// float up = dot(Normal, vec3(0,1,0));
	// float range = smoothstep(0.7, 0.75, up) - smoothstep(0.8, 0.9, up);
	// FragColor = baseColor+3000*mix(vec4(0,0,0, 1), vec4(5,0,0, 1), step(0.0, (-curvature(FragPos, Normal))) *  range  );

	// float curve = curvature(Normal);
	// smoothstep(0.5, 1.0, curve) * 1.0 - smoothstep

	NormalOut = vec4(normalize(mat3(view) * perturbedNorm), primaryShadow);
	AlbedoOut = vec4(albedo, 1.0);

	vec2 a = (CurPosition.xy / CurPosition.w) * 0.5 + 0.5;
	vec2 b = (PrevPosition.xy / PrevPosition.w) * 0.5 + 0.5;
	Velocity = vec4(a - b, roughness, metallic);
}