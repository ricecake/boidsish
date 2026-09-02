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


#define ADSR_FADE(t, start, attack, sustain, release) \
    (smoothstep(start, start + attack, t) * (1.0 - smoothstep(start + attack + sustain, start + attack + sustain + release, t)))


#define EVAL_LOD_OPTIMIZED(OUT_VAR, FUNC, TRANS_LEN, SEG_LEN, CUR_LEN) \
    { \
        float _layer = floor((CUR_LEN) / (SEG_LEN)); \
        float _local = mod((CUR_LEN), (SEG_LEN)); \
        float _blend = smoothstep((SEG_LEN) - (TRANS_LEN), (SEG_LEN), _local); \
        OUT_VAR = FUNC(_layer); \
        if (_blend > 0.0) { \
            OUT_VAR = mix(OUT_VAR, FUNC(_layer + 1.0), _blend); \
        } \
    }

// FUNC: A function that takes a float layer_index and returns your procedural texture (float, vec2, vec4, etc.)
#define LOD_BLEND(FUNC, TRANS_LEN, SEG_LEN, CUR_LEN) \
    mix( \
        FUNC(floor((CUR_LEN) / (SEG_LEN))), \
        FUNC(floor((CUR_LEN) / (SEG_LEN)) + 1.0), \
        smoothstep((SEG_LEN) - (TRANS_LEN), (SEG_LEN), mod((CUR_LEN), (SEG_LEN))) \
    )


struct TerrainMaterial {
	vec3  albedo;
	float roughness;
	float metallic;
	float normalScale;
	float normalStrength;
	GlintProperties glint;
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

vec2 worldToFlat(
	vec3  worldPos,
	vec3  worldNormal
) {
	// 1. Construct the TBN frame
	vec3 N = normalize(worldNormal);

	// Choose an 'up' vector, switching to X-axis if the normal is perfectly vertical
	vec3 referenceUp = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);

	vec3 T = normalize(cross(referenceUp, N));
	vec3 B = normalize(cross(N, T));

	// 2. Project world position into 2D surface space
	vec2 surfacePos = vec2(dot(worldPos, T), dot(worldPos, B));

	return surfacePos;
}

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
	mat.glint.intensity = 0.0; mat.glint.density = 0.0; mat.glint.micro_roughness = 0.0; mat.glint.filter_size = 0.0; mat.glint.scale = 1.0;
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
	mat.glint.intensity = 0.0; mat.glint.density = 0.0; mat.glint.micro_roughness = 0.0; mat.glint.filter_size = 0.0; mat.glint.scale = 1.0;
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

		GlintProperties sandGlint;
		sandGlint.intensity = 0.7; sandGlint.density = 2500.0; sandGlint.micro_roughness = 0.012; sandGlint.filter_size = 0.7; sandGlint.scale = 1.2;
		mat.glint = sandGlint;

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
	mat.glint.intensity = 0.0; mat.glint.density = 0.0; mat.glint.micro_roughness = 0.0; mat.glint.filter_size = 0.0; mat.glint.scale = 1.0;
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

float gate(float min_val, float max_val, float val) {
	return val * smoothstep(min_val, max_val, val);
}

float threshold(float minv, float maxv, float width, float val) {
	float halfWidth = width * 0.5;
	return val * smoothstep(minv - halfWidth, minv+halfWidth, val) * (1.0 - smoothstep(maxv - halfWidth, maxv + halfWidth, val));
}
/**
 * Process water layers (e.g. wet surfaces, refractions).
 */
void processWaterLayer(vec3 norm, float dist, float fade) {
    // Calculate physically consistent water depth and shallow shore factor
    float waterDepth = max(vIsWater * 5.0 + max(FragPos.y, 0.0), 0.05);
    float shallowFactor = clamp(1.0 - waterDepth / 3.0, 0.0, 1.0);

    // Dynamic water roughness and metallic model
    float waterRoughness = mix(0.05, 0.15, shallowFactor);
    float waterMetallic = 0.0;

    // Refraction ray calculation through water surface
    vec3 incidentDir = normalize(FragPos - viewPos);
    vec3 normal = normalize(norm);
    float eta = 1.0 / 1.33; // air to water IOR ratio

    vec3 refractionDir = refract(incidentDir, normal, eta);
    vec2 uvOffset = (refractionDir.xz / max(abs(refractionDir.y), 0.001)) * waterDepth;

    // Refracted underwater floor pebbles
    vec2 pebbleUV = FragPos.xz + uvOffset;
    vec3 pebbleVor = voronoi(pebbleUV);
    float pebbleRand = random(pebbleVor.xy);
    float pebbleDist = pebbleVor.z;

    vec3 pebbleColor;
    if (pebbleRand < 0.25) {
        pebbleColor = vec3(0.42, 0.44, 0.46);
    } else if (pebbleRand < 0.5) {
        pebbleColor = vec3(0.38, 0.35, 0.32);
    } else if (pebbleRand < 0.75) {
        pebbleColor = vec3(0.48, 0.35, 0.32);
    } else {
        pebbleColor = vec3(0.65, 0.66, 0.64);
    }

    vec2 pebbleOffset = (pebbleUV - pebbleVor.xy) * 2.0;
    float pebbleLight = clamp(dot(normalize(vec3(pebbleOffset, 1.0)), normalize(vec3(-0.5, 0.5, 1.0))), 0.0, 1.0);
    vec3 shadedPebble = pebbleColor * (0.35 + 0.65 * pebbleLight);
    shadedPebble *= smoothstep(0.8, 0.4, pebbleDist);

    // Wave-correlated subsurface caustics driven by surface wave distortion & light refraction
    vec2 causticUV = FragPos.xz * 0.7 + norm.xz * (waterDepth * 0.8) + uvOffset * 0.4;
    float c1 = fastSimplex3d(vec3(causticUV + vec2(time * 0.5, time * 0.3), time * 0.2));
    float c2 = fastSimplex3d(vec3(causticUV * 1.4 - vec2(time * 0.3, time * 0.4), time * 0.25));
    float causticPattern = pow(clamp(1.0 - abs(c1 + c2), 0.0, 1.0), 3.5);
    vec3 caustics = causticPattern * vec3(1.2, 1.4, 1.5) * exp(-waterDepth * 0.35);
    shadedPebble += caustics;

    // Procedural fish swarm in water column
    vec2 fishGridScale = vec2(3.5);
    vec2 fishCell = floor((FragPos.xz + uvOffset * 0.3) / fishGridScale);
    vec2 cellRand = hash2(fishCell);

    if (cellRand.x > 0.35) { // ~65% probability of fish in grid cell
        float fishTime = time * (0.9 + cellRand.y * 0.7) + cellRand.x * 6.2831;
        vec2 cellCenter = (fishCell + 0.5) * fishGridScale;
        vec2 fishOrbit = cellCenter + vec2(cos(fishTime), sin(fishTime * 1.2)) * (0.7 + cellRand.y * 0.8);

        float fishDepth = 0.4 + cellRand.y * 2.2;
        if (fishDepth < waterDepth) {
            vec2 fishVel = vec2(-sin(fishTime), 1.2 * cos(fishTime * 1.2));
            float fishAngle = atan(fishVel.y, fishVel.x);

            vec2 pRel = (FragPos.xz + uvOffset * 0.3) - fishOrbit;
            mat2 rot = mat2(cos(fishAngle), sin(fishAngle), -sin(fishAngle), cos(fishAngle));
            vec2 pFish = rot * pRel;

            // Tail wiggle animation along teardrop axis
            float wiggle = sin(fishTime * 9.0 + pFish.x * 12.0) * 0.035 * smoothstep(0.1, -0.4, pFish.x);
            pFish.y -= wiggle;

            // Teardrop shape evaluation
            float fishLen = 0.45 * (0.7 + cellRand.y * 0.6);
            float headRadius = 0.11 * (0.7 + cellRand.x * 0.5);

            float normX = clamp((pFish.x + fishLen * 0.5) / fishLen, 0.0, 1.0);
            float bodyWidth = headRadius * sin(normX * 3.14159) * (1.0 - 0.25 * normX);
            float fishSdf = abs(pFish.y) - bodyWidth;

            float fishMask = smoothstep(0.02, -0.01, fishSdf) * step(-fishLen * 0.5, pFish.x) * step(pFish.x, fishLen * 0.5);

            if (fishMask > 0.0) {
                vec3 fishBaseColor = palette(
                    cellRand.x,
                    vec3(0.5, 0.5, 0.5),
                    vec3(0.5, 0.5, 0.5),
                    vec3(1.0, 0.8, 0.5),
                    vec3(0.0, 0.33, 0.67)
                );
                // Depth-attenuated fish shading
                vec3 fishTransmission = exp(-fishDepth * vec3(1.0, 0.35, 0.1));
                vec3 shadedFish = fishBaseColor * fishTransmission * (0.6 + 0.4 * clamp(pFish.y / max(bodyWidth, 0.001), 0.0, 1.0));
                shadedPebble = mix(shadedPebble, shadedFish, fishMask * exp(-fishDepth * 0.3));
            }
        }
    }

    // Volumetric Absorption (Beer-Lambert Law)
    vec3 scatterCoefficients = vec3(1.1, 0.35, 0.08);
    vec3 transmission = exp(-waterDepth * scatterCoefficients);

    // Apply volumetric transmission to underwater view
    vec3 underwaterView = shadedPebble * transmission;

    // Shallow shore water coloration and deep water tint
    vec3 waterTint = mix(vec3(0.02, 0.25, 0.45), vec3(0.1, 0.45, 0.55), shallowFactor);
    vec3 surfaceAlbedo = underwaterView + waterTint;

    // Wave crest foam & shoreline wash foam
    float waveSteepness = clamp(1.0 - norm.y, 0.0, 1.0);
    float crestFoam = smoothstep(0.15, 0.38, waveSteepness) * smoothstep(0.0, 0.25, FragPos.y + 0.05);
    float shoreWash = smoothstep(0.65, 1.0, shallowFactor) * (0.5 + 0.5 * sin(time * 3.5 + FragPos.x * 0.4 + FragPos.z * 0.4));
    float totalFoam = clamp(crestFoam * 1.6 + shoreWash * 0.7, 0.0, 1.0);

    vec3 foamColor = vec3(0.95, 0.98, 1.0);
    surfaceAlbedo = mix(surfaceAlbedo, foamColor, totalFoam);
    waterRoughness = mix(waterRoughness, 0.6, totalFoam * 0.8);

    float primaryShadow;
    GlintProperties waterGlint;
    waterGlint.intensity = 0.85;
    waterGlint.density = 4500.0;
    waterGlint.micro_roughness = 0.003;
    waterGlint.filter_size = 0.7;
    waterGlint.scale = 1.0;
    vec3 lighting = apply_lighting_pbr(FragPos, norm, surfaceAlbedo, waterRoughness, waterMetallic, 1.0, primaryShadow, waterGlint).rgb;

    // Add foam specular contribution
    lighting += foamColor * totalFoam * 0.25 * primaryShadow;

    vec3 final_color = lighting;

    FragColor = vec4(final_color, 1.0);

    NormalOut = vec4(normalize(mat3(view) * norm), primaryShadow);
    AlbedoOut = vec4(surfaceAlbedo, 1.0);
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
	finalMaterial.glint = mixGlintProperties(biomeMat.glint, cliffMat.glint, ctx.cliffMask);

	// Large-scale macro color shifts
	finalMaterial.albedo *= (1.0 + largeNoise * 0.12);
	return finalMaterial;
}

/**
 * Redone grass styling helper driven by TerrainContext.
 */
TerrainMaterial applyGrassStylingOld(
	TerrainContext ctx,
	TerrainMaterial mat,
	float blueNoise,
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
		vec3 grassColor = mix(colorA, colorB, smoothstep(0.0, blueNoise, ctx.biomeT));

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
 * Redone grass styling helper driven by TerrainContext.
 */
TerrainMaterial applyGrassStyling(
	vec3 pos,
	TerrainContext ctx,
	TerrainMaterial mat,
	float blueNoise,
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
		vec3 grassColor = mix(colorA, colorB, smoothstep(0.0, blueNoise, ctx.biomeT));

		// Apply effect only on relatively flat surfaces where grass would grow
		float grassMask = smoothstep(0.7, 0.8, perturbedNorm.y) * clamp(interpolatedDensity, 0.0, 1.0);

		// AO baseline shift - darken dense grass areas
		grassAO = grassMask * 0.75;

		float distanceFactor = smoothstep(200.0, 350.0, ctx.dist);

		perturbedNorm = mix(perturbedNorm, vec3(0.0, 1.0, 0.0), interpolatedDensity * distanceFactor);
		perturbedNorm = normalize(perturbedNorm);

		vec3 undersideColor = grassColor * 1.25 + vec3(0.05, 0.05, 0.0);
		vec3 dynamicGrassColor = mix(grassColor, undersideColor, blueNoise);

		float tuft = fastWorley3d(pos / ((125.0+25*blueNoise) * worldScale));

		mat.albedo = mix(mat.albedo, dynamicGrassColor, tuft);
		mat.albedo = mix(mat.albedo, dynamicGrassColor, tuft);

		float floorTexture = pow(blueNoise, 1.0+blueNoise);

		floorTexture = mix(1.0, floorTexture, (1.0 - smoothstep(0.0, 100.0, ctx.dist)));
		mat.albedo *= floorTexture;

		mat.roughness = mix(mat.roughness, clamp(mat.roughness, 0.0, 1.0), distanceFactor);
	}
	return mat;
}


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
    // Early exit combined to save cycles
    if (perturbFactor < 0.1 || normalStrength <= 0.0 || (ctx.dist + 50.0 * largeNoise) >= 200.0 || waterEffect > 0.0) {
        return;
    }

    float roughnessStrength = smoothstep(0.1, 1.0, perturbFactor) * normalStrength;

    // 1. Base Coordinate Space
    vec3 basePos = (FragPos / worldScale) * normalScale * 0.05;

    // 2. Domain Warping based on TerrainContext
    // Compress Y on cliffs to create horizontal rock strata
    // basePos.y *= mix(1.0, 0.15, ctx.cliffMask);

    // Increase frequency in freezing zones for icy/crystalline crunch
    basePos *= mix(1.0, 3.5, ctx.freezingScale);

    // Optional: Add large-scale variance to break up grid alignment
    basePos += largeNoise * 0.1;

    // 3. Single Noise Evaluation (Branchless)
    float n = fastWarpedFbm3d(basePos)*0.5+0.5;

    // 4. Screen-Space Derivatives
    vec3 dPdx = dFdx(basePos);
    vec3 dPdy = dFdy(basePos);
    float dNdx = dFdx(n);
    float dNdy = dFdy(n);

    // 5. Compute Surface Gradient
    vec3 R1 = cross(dPdy, perturbedNorm);
    vec3 R2 = cross(perturbedNorm, dPdx);
    vec3 surfGrad = (R1 * dNdx + R2 * dNdy) / (dot(dPdx, R1) + 0.00001);

    // 6. Apply Perturbation
    perturbedNorm = normalize(perturbedNorm - surfGrad * roughnessStrength);

    // Toksvig-like Adjustment
    float variance = dot(surfGrad, surfGrad) * roughnessStrength;
    roughness = sqrt(clamp(roughness * roughness + variance * 0.25, 0.0, 1.0));
}

TerrainMaterial generateMaterial(TerrainContext ctx, float noise) {
	GlintProperties defaultGlint;
	defaultGlint.intensity = 0.0; defaultGlint.density = 0.0; defaultGlint.micro_roughness = 0.0; defaultGlint.filter_size = 0.0; defaultGlint.scale = 1.0;
	TerrainMaterial mat = TerrainMaterial(vec3(0,0.0,0), 0.0, 0.0, 1.0, 1.0, defaultGlint);

	// Balanced Roughness and Metallic Model across sand, rock, grass, snow and damp
	float snowFactor = max(ctx.freezingScale, smoothstep(HEIGHT_SNOW_START, HEIGHT_PEAK, ctx.perturbedHeight));
	float sandFactor = 1.0 - smoothstep(0.0, HEIGHT_BEACH_END, ctx.perturbedHeight);
	float rockFactor = ctx.cliffMask;

	float dampFactor = clamp(ctx.globalWetness + ctx.moisture * 0.4, 0.0, 1.0);

	float grassRough = mix(0.85, 0.65, dampFactor);
	float sandRough = mix(0.80, 0.40, dampFactor);
	float rockRough = mix(0.70, 0.25, dampFactor);
	float snowRough = 0.65;

	float blendedRough = grassRough;
	blendedRough = mix(blendedRough, rockRough, rockFactor);
	blendedRough = mix(blendedRough, sandRough, sandFactor);
	blendedRough = mix(blendedRough, snowRough, snowFactor);

	mat.roughness = clamp(blendedRough, 0.0, 1.0);
	mat.albedo = texture(u_terrainColorBlend, vec3(ctx.perturbedHeight/100.0, (ctx.moisture+clamp(ctx.substrate, 0, 1.0)/2.0), mat.roughness)).rgb;
	mat.metallic = 0.0;

	mat.albedo = mix(mat.albedo, mat.albedo * 0.55, noise);
	mat.albedo = applyErosionColorMappingDefault(mat.albedo, vRidgeMap, vErosionDelta);

	GlintProperties grassGlint;
	grassGlint.intensity = 0.0; grassGlint.density = 0.0; grassGlint.micro_roughness = 0.0; grassGlint.filter_size = 0.0; grassGlint.scale = 1.0;

	GlintProperties rockGlint;
	rockGlint.intensity = 0.0; rockGlint.density = 0.0; rockGlint.micro_roughness = 0.0; rockGlint.filter_size = 0.0; rockGlint.scale = 1.0;

	GlintProperties sandGlint;
	sandGlint.intensity = 0.7; sandGlint.density = 2500.0; sandGlint.micro_roughness = 0.012; sandGlint.filter_size = 0.7; sandGlint.scale = 1.2;

	GlintProperties snowGlint;
	snowGlint.intensity = 1.0; snowGlint.density = 7000.0; snowGlint.micro_roughness = 0.005; snowGlint.filter_size = 0.7; snowGlint.scale = 1.0;

	GlintProperties blendedGlint = grassGlint;
	blendedGlint = mixGlintProperties(blendedGlint, rockGlint, rockFactor);
	blendedGlint = mixGlintProperties(blendedGlint, sandGlint, sandFactor);
	blendedGlint = mixGlintProperties(blendedGlint, snowGlint, snowFactor);

	mat.glint = blendedGlint;

	return mat;
}

float calculateAntiAliasedFBM(vec3 pos, float baseFreq, int octaves) {
    float pixelFootprint = length(fwidth(pos));

	// The Nyquist limit is 2 pixels per cycle.
	// We fade the octave out smoothly as its period approaches this limit.
	float nyquistLimit = pixelFootprint * 2.0;

    float value = 0.0;
    float amplitude = 1.0;
    float frequency = baseFreq;
    float totalAmplitude = 0.0;

    for (int i = 0; i < octaves; i++) {
        float period = 1.0 / frequency;

        // Transition band: Fade out when the period is between 1x and 4x the pixel footprint
        float fade = smoothstep(nyquistLimit * 0.5, nyquistLimit * 2.0, period);

        // Early exit optimization: If the frequency is entirely sub-pixel, stop calculating
        if (fade <= 0.0) break;

        // Evaluate the noise function
		value += amplitude * fade * (fastBlueNoise(pos.xz * frequency, 0) * 0.5 + 0.5);
        totalAmplitude += amplitude * fade;

        // Standard fBm progression
        frequency *= 2.0; // Lacunarity
        amplitude *= 0.5; // Gain
    }

    return value / totalAmplitude;
}

// Standard 2D hash for Voronoi seeds
vec2 hash2(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * vec3(.1031, .1030, .0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.xx + p3.yz) * p3.zy);
}

// Placeholder: Replace with your engine's existing smooth noise (Simplex/Perlin)
vec2 warpNoise2D(vec2 p) {
    // Basic value noise for demonstration
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash2(i + vec2(0.0,0.0)), hash2(i + vec2(1.0,0.0)), u.x),
               mix(hash2(i + vec2(0.0,1.0)), hash2(i + vec2(1.0,1.0)), u.x), u.y);
}


struct CellularBayerResult {
    float ditherVal;     // Combined Bayer + Worley dither value normalized in [0, 1]
    float bayerVal;      // Raw Bayer matrix value in [0, 1]
    float minDist;       // Squared distance to closest cell feature point
    vec2  closestCellId; // Cell grid coordinate
    vec2  localOffset;   // Vector offset from feature point
};

CellularBayerResult evalCellularBayer(vec2 uv, float warpStrength) {
    CellularBayerResult res;
    vec2 warpOffset = (warpStrength > 0.0) ? (warpNoise2D(uv * 0.5 + 13.37) - 0.5) * warpStrength : vec2(0.0);
    vec2 warpedUV = uv + warpOffset;

    vec2 grid = floor(warpedUV);
    vec2 local = fract(warpedUV);

    float minDist = 10.0;
    vec2 closestCellId = vec2(0.0);
    vec2 closestDiff = vec2(0.0);

    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            vec2 neighbor = vec2(float(x), float(y));
            vec2 featurePt = hash2(grid + neighbor);

            vec2 diff = neighbor + featurePt - local;
            float dist = dot(diff, diff);

            if (dist < minDist) {
                minDist = dist;
                closestCellId = grid + neighbor;
                closestDiff = diff;
            }
        }
    }

    ivec2 cellCoord = ivec2(closestCellId);
    cellCoord = ((cellCoord % 8) + 8) % 8;

    const float bayer[64] = float[64](
        0.0, 32.0,  8.0, 40.0,  2.0, 34.0, 10.0, 42.0,
        48.0, 16.0, 56.0, 24.0, 50.0, 18.0, 58.0, 26.0,
        12.0, 44.0,  4.0, 36.0, 14.0, 46.0,  6.0, 38.0,
        60.0, 28.0, 52.0, 20.0, 62.0, 30.0, 54.0, 22.0,
         3.0, 35.0, 11.0, 43.0,  1.0, 33.0,  9.0, 41.0,
        51.0, 19.0, 59.0, 27.0, 49.0, 17.0, 57.0, 25.0,
        15.0, 47.0,  7.0, 39.0, 13.0, 45.0,  5.0, 37.0,
        63.0, 31.0, 55.0, 23.0, 61.0, 29.0, 53.0, 21.0
    );

    float bayerVal = bayer[cellCoord.y * 8 + cellCoord.x] / 64.0;

    // Normalize combined state into [0, 1] range so thresholds transition cleanly
    float erosionTightness = 0.8;
    float ditherVal = clamp((bayerVal + sqrt(minDist) * erosionTightness) / 1.5, 0.0, 1.0);

    res.ditherVal = ditherVal;
    res.bayerVal = bayerVal;
    res.minDist = minDist;
    res.closestCellId = closestCellId;
    res.localOffset = closestDiff;
    return res;
}

vec3 cellularBayerDither(vec2 uv, vec3 valA, vec3 valB, float threshold, float warpStrength) {
    CellularBayerResult cb = evalCellularBayer(uv, warpStrength);
    float tRemapped = mix(-0.1, 1.1, clamp(threshold, 0.0, 1.0));
    return (tRemapped > cb.bayerVal) ? valA : valB;
}

vec3 cellularErosionDither(vec2 uv, vec3 valA, vec3 valB, float threshold, float warpStrength) {
    CellularBayerResult cb = evalCellularBayer(uv, warpStrength);

    // Smoothly re-map threshold from [0, 1] to [-0.15, 1.15] so threshold==0 guarantees 100% valB
    // and threshold==1 guarantees 100% valA without premature resetting or clamping artifacts.
    float tRemapped = mix(-0.15, 1.15, clamp(threshold, 0.0, 1.0));
    float mixFactor = smoothstep(cb.ditherVal - 0.1, cb.ditherVal + 0.1, tRemapped);

    return mix(valB, valA, mixFactor);
}

TerrainMaterial cellularErosionDitherMaterial(vec2 uv, TerrainMaterial matA, TerrainMaterial matB, float threshold, float warpStrength) {
    CellularBayerResult cb = evalCellularBayer(uv, warpStrength);
    float tRemapped = mix(-0.15, 1.15, clamp(threshold, 0.0, 1.0));
    float mixFactor = smoothstep(cb.ditherVal - 0.1, cb.ditherVal + 0.1, tRemapped);

    TerrainMaterial res;
    res.albedo = mix(matB.albedo, matA.albedo, mixFactor);
    res.roughness = mix(matB.roughness, matA.roughness, mixFactor);
    res.metallic = mix(matB.metallic, matA.metallic, mixFactor);
    res.normalScale = mix(matB.normalScale, matA.normalScale, mixFactor);
    res.normalStrength = mix(matB.normalStrength, matA.normalStrength, mixFactor);
    res.glint = mixGlintProperties(matB.glint, matA.glint, mixFactor);
    return res;
}

// Dedicated Ground Material Renderers

TerrainMaterial renderGrassGround(vec3 pos, vec3 norm, TerrainContext ctx, float bayerVal, float blueNoise) {
    TerrainMaterial mat;
    mat.glint.intensity = 0.0; mat.glint.density = 0.0; mat.glint.micro_roughness = 0.0; mat.glint.filter_size = 0.0; mat.glint.scale = 1.0;
    mat.metallic = 0.0;
    mat.normalScale = 12.0;
    mat.normalStrength = 0.08;

    vec3 colLush = mix(COL_GRASS_LUSH, vec3(0.28, 0.52, 0.18), bayerVal * 0.5);
    vec3 colDry  = mix(COL_GRASS_DRY, vec3(0.52, 0.45, 0.22), bayerVal * 0.5);
    vec3 colAlpine = mix(COL_ALPINE_MEADOW, vec3(0.42, 0.55, 0.32), bayerVal * 0.5);

    vec3 grassColor = mix(colLush, colDry, clamp(1.0 - ctx.moisture, 0.0, 1.0));
    grassColor = mix(grassColor, colAlpine, smoothstep(HEIGHT_FOREST_END, HEIGHT_TREELINE, ctx.perturbedHeight));

    if (bayerVal > 0.82 && ctx.moisture > 0.45) {
        vec3 flowerColor = mix(vec3(0.85, 0.75, 0.2), vec3(0.8, 0.3, 0.5), sin(pos.x * 0.5 + pos.z * 0.5) * 0.5 + 0.5);
        grassColor = mix(grassColor, flowerColor, 0.4);
    }

    float tuft = fastWorley3d(pos / ((125.0 + 25.0 * blueNoise) * worldScale));
    grassColor = mix(grassColor, grassColor * 1.2, tuft * 0.5);

    mat.albedo = grassColor * (0.8 + 0.4 * blueNoise);
    mat.roughness = mix(0.85, 0.65, ctx.globalWetness);
    return mat;
}

TerrainMaterial renderStonesGround(vec3 pos, vec3 norm, TerrainContext ctx, vec2 uv, float warpStrength) {
    TerrainMaterial mat;
    mat.glint.intensity = 0.0; mat.glint.density = 0.0; mat.glint.micro_roughness = 0.0; mat.glint.filter_size = 0.0; mat.glint.scale = 1.0;
    mat.metallic = 0.02;
    mat.normalScale = 25.0;
    mat.normalStrength = 0.18;

    CellularBayerResult cb = evalCellularBayer(uv * 12.0, warpStrength);

    vec3 stoneColor;
    if (cb.bayerVal < 0.25) {
        stoneColor = vec3(0.42, 0.44, 0.46); // slate gray river stone
    } else if (cb.bayerVal < 0.50) {
        stoneColor = vec3(0.38, 0.35, 0.32); // brown/tan river stone
    } else if (cb.bayerVal < 0.75) {
        stoneColor = vec3(0.48, 0.35, 0.32); // terracotta brick stone
    } else {
        stoneColor = vec3(0.65, 0.66, 0.64); // light quartz stone
    }

    float pebbleDist = sqrt(cb.minDist);
    float gapShading = smoothstep(0.8, 0.3, pebbleDist);

    mat.albedo = stoneColor * (0.35 + 0.65 * gapShading);
    mat.roughness = mix(0.65, 0.35, ctx.globalWetness * gapShading);
    return mat;
}

TerrainMaterial renderSolidRockGround(vec3 pos, vec3 norm, TerrainContext ctx, float largeNoise) {
    TerrainMaterial mat;
    mat.glint.intensity = 0.0; mat.glint.density = 0.0; mat.glint.micro_roughness = 0.0; mat.glint.filter_size = 0.0; mat.glint.scale = 1.0;
    mat.metallic = 0.0;
    mat.normalScale = 40.0;
    mat.normalStrength = 0.15;

    CellularBayerResult cb = evalCellularBayer(pos.xz * 0.15, 0.2);

    vec3 baseRock = mix(COL_ROCK_BROWN, COL_ROCK_GREY, cb.bayerVal);
    baseRock = mix(baseRock, COL_ROCK_DARK, largeNoise * 0.3 + (1.0 - ctx.slope) * 0.2);

    float stripeNoise = fastRidge3d(pos * 0.18) * 0.5 + 0.5;
    float stripeIntensity = smoothstep(0.82, 0.94, stripeNoise);

    vec3 stripeColor = (cb.bayerVal < 0.5) ? vec3(0.96, 0.96, 0.93) : vec3(0.68, 0.16, 0.10);
    float stripeMetallic = (cb.bayerVal < 0.5) ? 0.45 : 0.55;
    float stripeRoughness = (cb.bayerVal < 0.5) ? 0.18 : 0.35;

    mat.albedo = mix(baseRock, stripeColor, stripeIntensity * ctx.cliffMask);
    mat.metallic = mix(0.0, stripeMetallic, stripeIntensity * ctx.cliffMask);
    mat.roughness = mix(mix(0.70, 0.35, ctx.globalWetness), stripeRoughness, stripeIntensity * ctx.cliffMask);

    return mat;
}

TerrainMaterial renderSandSnowGround(vec3 pos, vec3 norm, TerrainContext ctx, float sandFactor, float snowFactor) {
    TerrainMaterial mat;
    mat.metallic = 0.0;

    CellularBayerResult cb = evalCellularBayer(pos.xz * 0.2, 0.1);

    vec3 sandColor = mix(COL_SAND_DRY, COL_SAND_WET, clamp(ctx.moisture + ctx.globalWetness * 0.5, 0.0, 1.0));
    sandColor *= (0.9 + 0.2 * cb.bayerVal);

    vec3 snowColor = mix(COL_SNOW_OLD, COL_SNOW_FRESH, cb.bayerVal);

    float sandSnowThreshold = clamp(snowFactor / max(0.001, sandFactor + snowFactor), 0.0, 1.0);
    float tRemapped = mix(-0.15, 1.15, sandSnowThreshold);
    float blendFactor = (sandFactor <= 0.001) ? 1.0 : ((snowFactor <= 0.001) ? 0.0 : smoothstep(cb.ditherVal - 0.1, cb.ditherVal + 0.1, tRemapped));

    mat.albedo = mix(sandColor, snowColor, blendFactor);
    mat.roughness = mix(mix(0.80, 0.40, ctx.globalWetness), 0.65, blendFactor);
    mat.normalScale = mix(30.0, 20.0, blendFactor);
    mat.normalStrength = mix(0.10, 0.05, blendFactor);

    GlintProperties sandGlint;
    sandGlint.intensity = 0.7; sandGlint.density = 2500.0; sandGlint.micro_roughness = 0.012; sandGlint.filter_size = 0.7; sandGlint.scale = 1.2;

    GlintProperties snowGlint;
    snowGlint.intensity = 1.0; snowGlint.density = 7000.0; snowGlint.micro_roughness = 0.005; snowGlint.filter_size = 0.7; snowGlint.scale = 1.0;

    mat.glint = mixGlintProperties(sandGlint, snowGlint, blendFactor);

    return mat;
}

// TerrainMaterial generateMaterial(TerrainContext ctx, float noise) {
//     // 1. Calculate factor weights for each ground type
//     float snowFactor = max(ctx.freezingScale, smoothstep(HEIGHT_SNOW_START, HEIGHT_PEAK, ctx.perturbedHeight));
//     float sandFactor = 1.0 - smoothstep(0.0, HEIGHT_BEACH_END, ctx.perturbedHeight);
//     float rockFactor = ctx.cliffMask;

//     // Stones/pebbles are prevalent in valleys/riverbeds and near beach shores
//     float stoneFactor = max(smoothstep(0.0, HEIGHT_BEACH_END * 1.5, ctx.perturbedHeight) * (1.0 - smoothstep(HEIGHT_BEACH_END * 1.5, HEIGHT_LOWLAND_END, ctx.perturbedHeight)),
//                             clamp(-ctx.valleyFactor * 0.6, 0.0, 0.8)) * (1.0 - rockFactor);

//     // Shared noise / cell evaluation for dithering
//     CellularBayerResult cb = evalCellularBayer(FragPos.xz * 0.1, 0.0);
//     float blueNoise = fastBlueNoise(FragPos.xz * 0.05, 0) * 0.5 + 0.5;

//     // 2. Render dedicated ground materials
//     TerrainMaterial matGrass = renderGrassGround(FragPos, Normal, ctx, cb.bayerVal, blueNoise);
//     TerrainMaterial matStones = renderStonesGround(FragPos, Normal, ctx, FragPos.xz * 0.1, 0.0);
//     TerrainMaterial matRock = renderSolidRockGround(FragPos, Normal, ctx, noise);
//     TerrainMaterial matSandSnow = renderSandSnowGround(FragPos, Normal, ctx, sandFactor, snowFactor);

//     // 3. Dither-blend ground materials sequentially using Bayer-Worley methods
//     TerrainMaterial result = matGrass;

//     if (stoneFactor > 0.01) {
//         result = cellularErosionDitherMaterial(FragPos.xz * 0.15, matStones, result, stoneFactor, 0.0);
//     }

//     float sandSnowWeight = max(sandFactor, snowFactor);
//     if (sandSnowWeight > 0.01) {
//         result = cellularErosionDitherMaterial(FragPos.xz * 0.12, matSandSnow, result, sandSnowWeight, 0.0);
//     }

//     if (rockFactor > 0.01) {
//         result = cellularErosionDitherMaterial(FragPos.xz * 0.18, matRock, result, rockFactor, 0.1);
//     }

//     vec3 blend3d = texture(u_terrainColorBlend, vec3(ctx.perturbedHeight / 100.0, (ctx.moisture + clamp(ctx.substrate, 0.0, 1.0) / 2.0), result.roughness)).rgb;
//     result.albedo = mix(result.albedo, blend3d, 0.35);
//     result.albedo = mix(result.albedo, result.albedo * 0.75, noise * 0.5);
//     result.albedo = applyErosionColorMappingDefault(result.albedo, vRidgeMap, vErosionDelta);

//     return result;
// }

// vec3 cellularBayerDither(vec2 uv, vec3 valA, vec3 valB, float threshold, float warpStrength) {
//     // 1. Domain Warp: Perturb the UVs before evaluating the cellular grid
//     // We offset the noise by a constant to avoid symmetries with the main hash
//     vec2 warpOffset = (warpNoise2D(uv * 0.5 + 13.37) - 0.5) * warpStrength;
//     vec2 warpedUV = uv + warpOffset;

//     // 2. Evaluate Worley structure
//     vec2 grid = floor(warpedUV);
//     vec2 local = fract(warpedUV);

//     float minDist = 10.0;
//     vec2 closestCellId = vec2(0.0);

//     // Standard 3x3 neighbor search
//     for(int y = -1; y <= 1; y++) {
//         for(int x = -1; x <= 1; x++) {
//             vec2 neighbor = vec2(float(x), float(y));
//             vec2 featurePt = hash2(grid + neighbor);

//             vec2 diff = neighbor + featurePt - local;
//             float dist = dot(diff, diff);

//             if(dist < minDist) {
//                 minDist = dist;
//                 closestCellId = grid + neighbor;
//             }
//         }
//     }

//     // 3. 8x8 Bayer Lookup using the CELL ID, not the fragment
//     ivec2 cellCoord = ivec2(closestCellId);

//     // GLSL '%' operator mirrors on negatives. This double-modulo forces it to wrap correctly.
//     cellCoord = ((cellCoord % 8) + 8) % 8;

//     const float bayer[64] = float[64](
//         0.0, 32.0,  8.0, 40.0,  2.0, 34.0, 10.0, 42.0,
//         48.0, 16.0, 56.0, 24.0, 50.0, 18.0, 58.0, 26.0,
//         12.0, 44.0,  4.0, 36.0, 14.0, 46.0,  6.0, 38.0,
//         60.0, 28.0, 52.0, 20.0, 62.0, 30.0, 54.0, 22.0,
//          3.0, 35.0, 11.0, 43.0,  1.0, 33.0,  9.0, 41.0,
//         51.0, 19.0, 59.0, 27.0, 49.0, 17.0, 57.0, 25.0,
//         15.0, 47.0,  7.0, 39.0, 13.0, 45.0,  5.0, 37.0,
//         63.0, 31.0, 55.0, 23.0, 61.0, 29.0, 53.0, 21.0
//     );

//     float bayerVal = bayer[cellCoord.y * 8 + cellCoord.x] / 64.0;

//     // 4. Evaluate Threshold
//     return (threshold > bayerVal) ? valA : valB;
// }

// vec3 cellularErosionDither(vec2 uv, vec3 valA, vec3 valB, float threshold, float warpStrength) {
//     // 1. Domain Warp
//     vec2 warpOffset = (warpNoise2D(uv * 0.5 + 13.37) - 0.5) * warpStrength;
//     vec2 warpedUV = uv + warpOffset;

//     // 2. Evaluate Worley structure
//     vec2 grid = floor(warpedUV);
//     vec2 local = fract(warpedUV);

//     float minDist = 10.0;
//     vec2 closestCellId = vec2(0.0);

//     for(int y = -1; y <= 1; y++) {
//         for(int x = -1; x <= 1; x++) {
//             vec2 neighbor = vec2(float(x), float(y));
//             vec2 featurePt = hash2(grid + neighbor);

//             vec2 diff = neighbor + featurePt - local;
//             float dist = dot(diff, diff);

//             if(dist < minDist) {
//                 minDist = dist;
//                 closestCellId = grid + neighbor;
//             }
//         }
//     }

//     // 3. 8x8 Bayer Lookup
//     ivec2 cellCoord = ivec2(closestCellId);
//     cellCoord = ((cellCoord % 8) + 8) % 8;

//     const float bayer[64] = float[64](
//         0.0, 32.0,  8.0, 40.0,  2.0, 34.0, 10.0, 42.0,
//         48.0, 16.0, 56.0, 24.0, 50.0, 18.0, 58.0, 26.0,
//         12.0, 44.0,  4.0, 36.0, 14.0, 46.0,  6.0, 38.0,
//         60.0, 28.0, 52.0, 20.0, 62.0, 30.0, 54.0, 22.0,
//          3.0, 35.0, 11.0, 43.0,  1.0, 33.0,  9.0, 41.0,
//         51.0, 19.0, 59.0, 27.0, 49.0, 17.0, 57.0, 25.0,
//         15.0, 47.0,  7.0, 39.0, 13.0, 45.0,  5.0, 37.0,
//         63.0, 31.0, 55.0, 23.0, 61.0, 29.0, 53.0, 21.0
//     );

//     float bayerVal = bayer[cellCoord.y * 8 + cellCoord.x] / 64.0;

//     // 4. Erosion Math
//     // Instead of evaluating (threshold > bayerVal), we use the distance to the center.
//     // We scale minDist by a tightness factor to control how fast the erosion happens.
//     float erosionTightness = 1.5;

//     // We combine the bayer value (acting as a base offset for the whole cell)
//     // with the distance (acting as a local modifier inside the cell).
//     float cellErosionState = bayerVal + (minDist * erosionTightness);

//     // Smoothstep gives a softer edge transition than a hard conditional
//     float mixFactor = smoothstep(threshold - 0.1, threshold + 0.1, cellErosionState);

//     return mix(valA, valB, mixFactor);
// }

void main() {
	if (uIsShadowPass) {
		return;
	}

	vec3  norm = normalize(Normal);
	float slope = dot(norm, vec3(0.0, 1.0, 0.0));

	float dist = length(FragPos.xz - viewPos.xz);
	float realDist = distance(FragPos, viewPos);

	if (dist > 650) {
		discard;
	}

	float baseFreq = 0.1 / worldScale;
	float largeNoise = fastWarpedFbm3d(FragPos * (baseFreq * 0.1));

	// WorleyData3D worley = worley3d_tiling_id((FragPos+largeNoise)*vec3(2.5, 0.04, 2.5), 16.0);
	// float terr = floor(100.0 * terraceSmooth(worley.f1_dist, 5, 0.5));

	// uint blue = mortonOwenScramble(uvec2(terr, terr), uint(0.0));

	// // FragColor = mix(vec4(0.3, 0.30, 30*step(5.0, mod(float(blue), 8)), 1.0), vec4(0.30, 0.30, 0.0, 1.0), mod(terr*2.0, 0.2));
	// FragColor = mix(vec4(1.0, 0.0, 0.0, 1.0), vec4(0.0, 1.0, 0.0, 1.0), float(mod(float(blue), 8)==0));
	// NormalOut = vec4(normalize(mat3(view) * Normal), 1.0);
	// AlbedoOut = FragColor;
	// Velocity = vec4(0.0);
	// return;

	if (vIsWater > 0.01) {
		processWaterLayer(norm, dist, largeNoise);
		return;
	}


	TerrainContext ctx = extractTerrainContext(
		FragPos, norm, largeNoise,
		vRidgeMap, vSubstrate,
		temperature, wetness,
		dist, realDist
	);

	TerrainMaterial finalMaterial = generateMaterial(ctx, largeNoise);

	applyDetailNormalPerturbation(
		ctx, perturbFactor, finalMaterial.normalStrength, finalMaterial.normalScale,
		0.0, largeNoise, norm, finalMaterial.roughness
	);

	float snowFactor = max(ctx.freezingScale, smoothstep(HEIGHT_SNOW_START, HEIGHT_PEAK, ctx.perturbedHeight));
	if (snowFactor > 0.0) {
		vec3 snowColor = mix(vec3(1.0), vec3(0.9, 0.95, 1.0 + 0.01 * 1.0), 0.5);
		// finalMaterial.albedo = mix(finalMaterial.albedo, snowColor, ctx.freezingScale);
		finalMaterial.albedo = cellularErosionDither(FragPos.xz, snowColor, finalMaterial.albedo, snowFactor, 0.0);
		finalMaterial.roughness = mix(finalMaterial.roughness, 0.85, ctx.freezingScale);
		finalMaterial.metallic = mix(finalMaterial.metallic, 0.0, ctx.freezingScale);

		GlintProperties snowGlint;
		snowGlint.intensity = 1.0; snowGlint.density = 7000.0; snowGlint.micro_roughness = 0.005; snowGlint.filter_size = 0.7; snowGlint.scale = 1.0;
		finalMaterial.glint = mixGlintProperties(finalMaterial.glint, snowGlint, snowFactor);
	}

	float primaryShadow;
	FragColor = apply_lighting_pbr(FragPos, norm, finalMaterial.albedo, finalMaterial.roughness, finalMaterial.metallic, 1.0, primaryShadow, finalMaterial.glint);
	// FragColor.b *= 1.0 + (0.2 * ctx.freezingScale * (1.0 - primaryShadow));

	NormalOut = vec4(normalize(mat3(view) * norm), primaryShadow);
	AlbedoOut = vec4(finalMaterial.albedo, 1.0);

	vec2 a = (CurPosition.xy / CurPosition.w) * 0.5 + 0.5;
	vec2 b = (PrevPosition.xy / PrevPosition.w) * 0.5 + 0.5;
	Velocity = vec4(a - b, finalMaterial.roughness, finalMaterial.metallic);
}

void main_old() {
	if (uIsShadowPass) {
		// Output only depth (handled by hardware)
		return;
	}

	vec3  norm = normalize(Normal);
	float slope = dot(norm, vec3(0.0, 1.0, 0.0));

	float dist = length(FragPos.xz - viewPos.xz);
	float realDist = distance(FragPos, viewPos);

	if (dist > 650) {
		discard;
	}

	float baseFreq = 0.1 / worldScale;
	// float stepDist = 50.0 * int(realDist / 50.0);
	// float freqScale = mix(1.0, 0.25, smoothstep(150.0, 160.0, stepDist + 100.0));
	float freqScale = 0.5;

	// ========================================================================
	// Noise Generation (Consolidated)
	// ========================================================================
	// Calculate unit vector towards the camera
	vec3 advectDir = realDist > 0.001 ? (FragPos - vec3(viewPos.x, 0.0, viewPos.z)) / realDist : vec3(0.0, 0.0, 1.0);

	// Proper 3D advected and warped lookups
	vec3 warpCoord1 = FragPos * (0.001 / worldScale) + vec3(0.0, time * 0.05, 0.0);
	vec3 warpCoord2 = FragPos * (0.015 / worldScale) - vec3(0.0, time * 0.03, 0.0);

	vec3 warp1 = fastCurl3d(warpCoord1);
	vec3 warp2 = warp1.yzx;

	float speed1 = 25.0 * worldScale;
	float speed2 = 18.0 * worldScale;

	vec3 p1 = FragPos + (advectDir * time * speed1) + 0.1*warp1;
	vec3 p2 = FragPos + (advectDir * time * speed2) + 0.2 * warp2;

	p1.y += time * 2.0 * worldScale; // slow vertical drift
	p2.y -= time * 1.5 * worldScale; // slow vertical drift in opposite direction

	float n_fade = fastSimplex3d(p1 / (250.0 * worldScale));
	float nightNoise = fastWorley3d(p2 / (125.0 * worldScale));

	float pixelFootprint = length(fwidth(FragPos.xz));
	float maxSafeFrequency = 1.0 / (2.0 * pixelFootprint);

	float largeNoise = fastWarpedFbm3d(FragPos * (baseFreq * 0.1));
	float blueNoise = 0.25*dot(vec4(1.0), fastBlueNoiseAll(FragPos.xz * min(maxSafeFrequency, baseFreq * 0.05)) *0.5 + 0.5  );

	// Consolidating additional procedural noises to minimize redundant execution calls
	float stripeNoise = fastRidge3d(FragPos * 0.18) * 0.5 + 0.5;
	float stripeType = fastSimplex3d(FragPos * 0.03);
	float pebblePatch = fastSimplex3d(FragPos * (0.05 / worldScale));

	float fade_start = 560.0 * worldScale;
	float fade_end = 570.0 * worldScale;
	float fade = 1.0 - smoothstep(fade_start, fade_end, realDist + n_fade * 40.0);

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

	TerrainMaterial finalMaterial = generateMaterial(ctx, largeNoise);

	// --- Shading Enhancements & Local Stone Detail Overlays ---

	// 1. More gray color to steep cliff faces
	float steepFactor = 1.0 - smoothstep(0.35, 0.7, ctx.slope);
	vec3 grayCliffColor = vec3(0.44, 0.45, 0.48) * (0.85 + 0.15 * n_fade);
	float cliffGrayBlend = clamp(ctx.cliffMask * steepFactor, 0.0, 1.0);
	finalMaterial.albedo = mix(finalMaterial.albedo, grayCliffColor, cliffGrayBlend);

	// 2. Outcroppings mostly boulder color
	float outcropMask = (ctx.biomeIdxA == 6) ? (1.0 - ctx.biomeT) : ((ctx.biomeIdxB == 6) ? ctx.biomeT : 0.0);
	float totalOutcropMask = smoothstep( 0.9, 1.0, ctx.slope) * outcropMask;
	vec3 boulderColor = vec3(0.35, 0.33, 0.31) * (0.85 + 0.15 * n_fade);
	finalMaterial.albedo = mix(finalMaterial.albedo, boulderColor, totalOutcropMask);

	// 3. White or red mineral stripes like deposits (on outcroppings and cliff faces)
	float stoneMask = max(totalOutcropMask, clamp(ctx.cliffMask * steepFactor, 0.0, 1.0));
	float stripeIntensity = smoothstep(0.85, 0.94, stripeNoise);
	float activeStripeMask = stripeIntensity * stoneMask;

	vec3 stripeColor = vec3(1.0);
	float stripeMetallic = 0.0;
	float stripeRoughness = 0.0;
	if (stripeType < 0.0) {
		// White Quartz deposit stripes
		stripeColor = vec3(0.96, 0.96, 0.93);
		stripeMetallic = 0.45;
		stripeRoughness = 0.18;
	} else {
		// Red Iron oxide deposit stripes
		stripeColor = vec3(0.68, 0.16, 0.1);
		stripeMetallic = 0.55;
		stripeRoughness = 0.35;
	}

	finalMaterial.albedo = mix(finalMaterial.albedo, stripeColor, activeStripeMask);
	finalMaterial.roughness = mix(finalMaterial.roughness, stripeRoughness, activeStripeMask);
	finalMaterial.metallic = mix(finalMaterial.metallic, stripeMetallic, activeStripeMask);

	// 4. Tangent-to-world-space normal perturbation for mineral stripes
	if (activeStripeMask > 0.01) {
		vec3 stripeGrad = fastCurl3d(FragPos * 0.18);
		vec3 stripePerturb = normalize(cross(norm, stripeGrad));
		norm = normalize(mix(norm, normalize(norm + stripePerturb * 0.35), activeStripeMask));
	}

	// 5. Sporadic beach pebbles near beaches
	float beachMask = 1.0 - smoothstep(0.0, HEIGHT_BEACH_END, ctx.perturbedHeight);
	float activePebbleMask = beachMask * smoothstep(0.1, 0.45, pebblePatch);

	if (activePebbleMask > 0.01) {
		vec2 beachPebbleUV = FragPos.xz * 12.0;
		vec3 beachPebbleVor = voronoi(beachPebbleUV);
		float beachPebbleRand = random(beachPebbleVor.xy);
		float beachPebbleDist = beachPebbleVor.z;

		float pebbleShape = smoothstep(0.65, 0.0, beachPebbleDist);
		float mixFactor = pebbleShape * activePebbleMask;

		if (mixFactor > 0.01) {
			vec3 beachPebbleColor;
			if (beachPebbleRand < 0.3) {
				beachPebbleColor = vec3(0.55, 0.54, 0.52); // grey pebble
			} else if (beachPebbleRand < 0.6) {
				beachPebbleColor = vec3(0.48, 0.44, 0.40); // brown/tan pebble
			} else if (beachPebbleRand < 0.8) {
				beachPebbleColor = vec3(0.65, 0.62, 0.58); // light quartz pebble
			} else {
				beachPebbleColor = vec3(0.35, 0.34, 0.34); // dark charcoal pebble
			}

			// Tangent-to-world-space normal perturbation for pebbles
			vec2 pebbleNormalOffset = (beachPebbleUV - beachPebbleVor.xy) * 2.5;
			vec3 localPebbleNormal = normalize(vec3(pebbleNormalOffset.x, 1.0, pebbleNormalOffset.y));

			vec3 N_base = normalize(norm);
			vec3 T = normalize(cross(N_base, abs(N_base.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0)));
			vec3 B = normalize(cross(N_base, T));
			vec3 worldPebbleNormal = normalize(T * localPebbleNormal.x + N_base * localPebbleNormal.y + B * localPebbleNormal.z);

			finalMaterial.albedo = mix(finalMaterial.albedo, beachPebbleColor, mixFactor);
			finalMaterial.roughness = mix(finalMaterial.roughness, 0.58, mixFactor);
			norm = normalize(mix(norm, worldPebbleNormal, mixFactor * 0.8));
		}
	}

	// ========================================================================
	// Advanced Erosion Filter Coloration
	// ========================================================================
	finalMaterial.albedo = applyErosionColorMappingDefault(finalMaterial.albedo, vRidgeMap, vErosionDelta);


	// // Extra variety for rocky/steep areas
	// float rockyVar = largeNoise;
	// float rockyMask = smoothstep(0.5, 0.2, ctx.slope);
	// finalMaterial.albedo = mix(finalMaterial.albedo, finalMaterial.albedo * (1.0 + rockyVar * 0.2), rockyMask);


	// ========================================================================
	// Grass-based Styling
	// ========================================================================
	float grassAO = 0.0;
	vec3 perturbedNorm = norm;
	finalMaterial = applyGrassStyling(
		FragPos,
		ctx, finalMaterial,
		blueNoise,
		grassAO, perturbedNorm
	);


	// Apply global wetness from precipitation
	// finalMaterial.albedo = mix(finalMaterial.albedo, finalMaterial.albedo * 0.5, ctx.globalWetness * 0.5);
	// finalMaterial.roughness = mix(finalMaterial.roughness, 0.1, ctx.globalWetness * 0.8);
	// ========================================================================
	// Rain & Running Water Effect
	// ========================================================================
	float waterEffect = 0.0;
	if (wetness > 0.6 && ctx.freezingScale < 0.1) {
		// Global rain wetness darkens terrain and reduces roughness
		float globalRainWetness = smoothstep(0.6, 1.0, wetness);
		finalMaterial.albedo = mix(finalMaterial.albedo, finalMaterial.albedo * 0.6, globalRainWetness * 0.4);
		finalMaterial.roughness = mix(finalMaterial.roughness, 0.08, globalRainWetness * 0.7);

		float rockSurface = 1.0 - smoothstep(0.2, 0.5, ctx.slope);
		rockSurface = max(rockSurface, smoothstep(0.2, -0.6, ctx.substrate));
		float waterFlowMask = rockSurface * smoothstep(0.6, 0.9, wetness);

		if (waterFlowMask > 0.01) {
			// High-frequency noise detail fades with distance to omit noisy ripples at far range
			float highFreqDetail = 1.0 - smoothstep(25.0, 60.0, ctx.realDist);
			float effectiveStreaks = 1.0;
			vec3 flowNoise = vec3(0.0);

			if (highFreqDetail > 0.001) {
				vec3 surfaceDown = vec3(0.0, -1.0, 0.0) - dot(vec3(0.0, -1.0, 0.0), norm) * norm;
				vec3 flowDir = normalize(surfaceDown + vec3(0.00001, 0.0, 0.0));
				float flowSpeed = 2.0;
				vec3 p_flow = (FragPos + -flowDir * time * flowSpeed) * 1.5;
				flowNoise = fastCurl3d(p_flow * 0.08);

				float streaks = smoothstep(0.3, 0.8, abs(flowNoise.x));
				streaks *= smoothstep(0.4, 0.6, fract(flowNoise.y * 0.5 + time * 0.8));
				effectiveStreaks = mix(1.0, streaks, highFreqDetail);
			}

			waterEffect = waterFlowMask * effectiveStreaks;
			finalMaterial.albedo = mix(finalMaterial.albedo, finalMaterial.albedo * 0.5, waterEffect * 0.5);
			finalMaterial.roughness = mix(finalMaterial.roughness, 0.02, waterEffect);
			finalMaterial.metallic = mix(finalMaterial.metallic, 0.1, waterEffect);

			if (waterEffect > 0.05 && highFreqDetail > 0.01) {
				vec3 flowNorm = normalize(flowNoise * 2.0 - 1.0);
				norm = normalize(mix(norm, flowNorm, waterEffect * 0.8 * highFreqDetail));
			}
		}
	}

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

	float snowFactor = max(ctx.freezingScale, smoothstep(HEIGHT_SNOW_START, HEIGHT_PEAK, ctx.perturbedHeight));

	if (ctx.freezingScale > 0.0) {
		// mix(vec3(1.0), vec3(0.9, 0.95, 1.0), 0.5)
		vec3 snowColor = mix(vec3(1.0), vec3(0.9, 0.95, 1.0 + 0.01 * grassAO), 0.5);
		albedo = mix(albedo, snowColor, ctx.freezingScale);
		roughness = mix(roughness, 0.85, ctx.freezingScale);
		metallic = mix(metallic, 0.0, ctx.freezingScale);
	}

	// Smooth transition of material properties near the far boundary to match sky.frag floor plane properties
	float floorBlend = smoothstep(0.0, 0.3, fade);
	albedo = mix(vec3(0.05, 0.05, 0.08), albedo, floorBlend);
	roughness = mix(0.05, roughness, floorBlend);
	metallic = mix(0.9, metallic, floorBlend);
	perturbedNorm = mix(vec3(0.0, 1.0, 0.0), perturbedNorm, floorBlend);

	if (snowFactor > 0.0) {
		GlintProperties snowGlint;
		snowGlint.intensity = 1.0; snowGlint.density = 7000.0; snowGlint.micro_roughness = 0.005; snowGlint.filter_size = 0.7; snowGlint.scale = 1.0;
		finalMaterial.glint = mixGlintProperties(finalMaterial.glint, snowGlint, snowFactor);
	}

	float primaryShadow;
	vec3 lighting = apply_lighting_pbr(FragPos, perturbedNorm, albedo, roughness, metallic, 1.0 - grassAO, primaryShadow, finalMaterial.glint).rgb;
	lighting.b *= 1.0 + (0.2 * ctx.freezingScale * (1.0 - primaryShadow));

	// ========================================================================
	// Neon 80s Synth Style (Night Theme)
	// ========================================================================
	// --- Grid logic ---
	float gridWarpStart = smoothstep(600, 650, distance(viewPos, FragPos));
	float gridWarpEnd = 1.0 - smoothstep(680, 700, distance(viewPos, FragPos));
	float gridWarp = max(1.0, FragPos.y *n_fade) * gridWarpStart * gridWarpEnd;

	float grid_spacing = 1.0;

	vec2  coord = (FragPos+0.1*warp1*gridWarp).xz / grid_spacing;
	vec2  f = max(fwidth(coord), vec2(0.0001));

	vec2  grid_minor = abs(fract(coord - 0.5) - 0.5) / f;
	float line_minor = min(grid_minor.x, grid_minor.y);
	float C_minor = 1.0 - min(line_minor, 1.0);

	vec2  grid_major = abs(fract(coord / 5.0 - 0.5) - 0.5) / f;
	float line_major = min(grid_major.x, grid_major.y);
	float C_major = 1.0 - min(line_major, 1.0);

	float intensity = max(C_minor, C_major * 1.5 * max(1.0, 500*n_fade*abs(gridWarp*curvature(FragPos, Normal)))  ) ;
	// vec3  grid_color = vec3(0.0, 0.8, 0.8) * intensity;
	vec3  grid_color = vec3(0.0, 0.8, 0.8) * intensity * 5000.0;


	// Synthwave grid lines
	float gridScale = 0.05; // Lines every 20 units
	vec2  gridUV = FragPos.xz * gridScale;

	vec2  grid = abs(fract(gridUV - 0.5) - 0.5) / (fwidth(gridUV) * 1.5);
	float line = min(grid.x, grid.y);
	float gridLine = 1.0 - smoothstep(0.0, 1.0, line);

	vec2  gridGlow = abs(fract(gridUV - 0.5) - 0.5) / (fwidth(gridUV) * 8.0);
	float lineGlow = min(gridGlow.x, gridGlow.y);
	float gridGlowFactor = 1.0 - smoothstep(0.0, 1.0, lineGlow);

	vec3 cyan = vec3(0.0, 1.0, 1.0)  * 2000.0;
	vec3 magenta = vec3(1.0, 0.0, 1.0) * 2000.0;

	// Calculate nightFade using the proper 3D advected nightNoise defined near the top of main
	float nightFade = smoothstep(fade_start - 10, fade_end, realDist + nightNoise * 100.0);

	// Temperature/cooling factor: 1.0 when fully digital, cools down to 0.0 near the solidification edge
	float temperatureFactor = smoothstep(0.0, 0.8, nightFade*2.0);

	// Hot magenta cools down to a deep, dark violet/blue
	vec3 coolColor = vec3(0.05, 0.0, 0.2) * 2000.0;
	vec3 dynamicMagenta = mix(coolColor, magenta, temperatureFactor);

	// Organic Worley-based magenta splotches centered on cell centers
	float splotchMask = smoothstep(0.5, 0.2, nightNoise);
	vec3 splotches = dynamicMagenta * splotchMask * 0.8;

	float heightGlow = smoothstep(0.0, 100.0 * worldScale, FragPos.y);
	vec3 fancyLight = mix(grid_color, gridLine * cyan * 0.8 + gridGlowFactor * dynamicMagenta * 0.4, fade);

	vec3 newLighting = mix(lighting, lighting * vec3(0.4, 0.1, 0.5), 0.7);
	newLighting += fancyLight;
	newLighting += dynamicMagenta * heightGlow * (0.8 + 0.2 * sin(time * 0.5));
	newLighting += splotches * (0.7 + 0.3 * sin(time * 0.8)); // Pulsating, advecting, cooling splotches

	// Blend the digital night theme between standard flat flooring lighting + grid_color and the full digital theme (newLighting) based on fade
	vec3 digitalTheme = mix(lighting + grid_color, newLighting, smoothstep(0.0, 0.5, fade));

	// Interpolate between PBR lighting and the digital night theme
	lighting = mix(mix(lighting, lighting+dynamicMagenta*gridGlowFactor, 2*nightFade), digitalTheme, nightFade*2);

	// Scanning print front band right at the solidification boundary representing hard light projector printing
	float printFront = smoothstep(0.0, 0.25, fade) * (1.0-smoothstep(0.25, 0.5, fade));
	printFront = pow(printFront, 2.0); // Sharpen the band
	vec3 printFrontColor = mix(cyan, magenta, 0.5) * 50.0 * printFront;

	// Scanning print front band right at the solidification boundary representing hard light projector printing
	float printFront2 = smoothstep(0.0, 0.25, 2.0*nightFade) * (1.0-smoothstep(0.25, 0.5, 2.0*nightFade));
	printFront2 = pow(printFront2, 2.0); // Sharpen the band
	vec3 printFrontColor2 = lighting * 50000.0 * printFront;

	// Apply print front glow on top
	lighting += printFrontColor;
	lighting += printFrontColor2;

	// ========================================================================
	// Distance Fade
	// ========================================================================
	// Alpha fade out over the last portion of the transition (e.g. as fade goes from 0.2 down to 0.0)
	float alphaFade = smoothstep(0.0, 0.2, fade);

	// Allow flat terrain near the far boundary to remain visible and fade out smoothly, masking any differences or seams
	float isFar = smoothstep(400.0 * worldScale, 500.0 * worldScale, realDist);
	float terrainFlatMask = mix(step(0.01, FragPos.y), 1.0, isFar);

	vec4 baseColor = vec4(lighting, fade * alphaFade * terrainFlatMask);

	// vec4 windy = computeWindAtPositionOptimized(FragPos, FragPos.y, norm);
	// vec3 windy = getWindAtPosition(FragPos);
	// windy.a =1;

	FragColor = mix(vec4(0.0, 0.7, 0.7, baseColor.a) * length(baseColor), baseColor, step(1.0, fade));
	// FragColor = vec4(100*normalize(mix(vec3(1,0,1), vec3(0,1,0), windy)), 1.0);
	// FragColor = mix(FragColor, vec4(windy, 1), 1.0);
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