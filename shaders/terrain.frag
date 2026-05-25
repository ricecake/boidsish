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

#define USE_TERRAIN_DATA
#include "helpers/erosion.glsl"
#include "helpers/fast_noise.glsl"
#include "helpers/terrain_noise.glsl"
#include "helpers/terrain_shadows.glsl"
#include "helpers/lighting.glsl"
#include "visual_effects.glsl"
// #include "helpers/noise.glsl"
#include "helpers/wind.glsl"
// #include "lygia/color/space/rgb2lab.glsl"
#include "lygia/color/palette.glsl"
#include "lygia/generative/voronoi.glsl"
#include "lygia/generative/random.glsl"


uniform bool uIsShadowPass = false;

// Biome texture array: RG8 - R=low_idx, G=t
uniform sampler2DArray uBiomeMap;
// Baked displacement: RGB=displacement.xyz, A=biome_override
uniform sampler2DArray u_displacementArray;
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

TerrainMaterial getRockTexture(vec3 baseColor, float height, float moisture, float noise) {
	TerrainMaterial mat;
	float realDist = distance(FragPos, viewPos);

	float h = height + noise * 8.0;

	vec2 rockFactors = (fastWorley3dID(FragPos/125) *0.5 +0.5);// * ((1- smoothstep(0, HEIGHT_BEACH_END, height)) + smoothstep(HEIGHT_TREELINE, HEIGHT_SNOW_START, height));
	float rockFactor = 1.0;//abs(rockFactors.x);
	float wetness = 1.0 - smoothstep(0.0, HEIGHT_BEACH_END, h) + moisture;

	mat.albedo = mix(baseColor, mix(COL_ROCK_GREY, COL_ROCK_DARK, noise * 0.3), rockFactor);
	mat.roughness = mix(0.9, 0.4, wetness);
	mat.normalScale = 40.0;
	mat.normalStrength = mix(0.1, 0.05, wetness);

	if (rockFactor  > 0) {
		// vec3 rockBoundary = voronoi((TexCoords+(noise*0.05))*int(50*mix(5, 0.1, smoothstep(50, 250, 20*int(realDist/20) ))));
		vec2 rockBoundary = fastWorley3dID(FragPos/125) * 0.5 + 0.5;

		float rockPalette = clamp(rockFactors.y, 0, 1);
		vec3 color = palette( // Make this a curl noise?
		     random(rockBoundary.y),
		     vec3(0.5, 0.5, 0.5), vec3(0.5, 0.5, 0.5),
		     mix(vec3(1.0, 1.0, 0.50), vec3(1.0, 1.0, 1.0), rockPalette),
		     mix(vec3(0.80, 0.90, 0.30), vec3(0.30, 0.20, 0.20), rockPalette)
		);
		// vec3 color = palette( // Make this a curl noise?
		// 	random(rockBoundary.y),
		// 	vec3(0.5, 0.5, 0.5), vec3(0.5, 0.5, 0.5),
		// 	mix(vec3(1.0, 1.0, 1.0), vec3(0.30, 0.20, 0.20), rockPalette),
		// 	mix(vec3(1.0, 1.0, 1.0), vec3(0.00, 0.10, 0.20), rockPalette)
		// );


		vec3 rockColor = color * ((1-smoothstep(0.0, max(0.75, random(rockBoundary.y)), rockBoundary.x)) * 0.8 + 0.2);

		mat.albedo = mix(mat.albedo, rockColor, smoothstep(0.5, 1, rockFactor));
		mat.roughness = mix(0.7, 0.1, wetness*smoothstep(0.01, 0.02, rockBoundary.x ));
		mat.normalScale = 40.0;
		mat.normalStrength = mix(0.1, 0.05, wetness);
	}
	return mat;
}

/**
 * Get the base biome material based on height
 */
TerrainMaterial getBiomeMaterial(float height, float moisture, float noise) {
	float realDist = distance(FragPos, viewPos);
	TerrainMaterial mat;
	mat.metallic = 0.0;
	// Distort height with noise for natural boundaries
	float h = height + noise * 8.0;


	// Beach zone (0 - 3)
	if (h < HEIGHT_BEACH_END) {
		vec2 rockFactors = (fastWorley3dID(FragPos/125) *0.5 +0.5) * ((1- smoothstep(0, HEIGHT_BEACH_END, height)) + smoothstep(HEIGHT_TREELINE, HEIGHT_SNOW_START, height));
		float rockFactor = rockFactors.x;
		float wetness = 1.0 - smoothstep(0.0, HEIGHT_BEACH_END, h) + moisture;

		mat.albedo = mix(COL_SAND_DRY, COL_SAND_WET, wetness);
		mat.roughness = mix(0.9, 0.4, wetness);
		mat.normalScale = 40.0;
		mat.normalStrength = mix(0.1, 0.05, wetness);

		if (rockFactor  > 0.5) {
			vec3 rockBoundary = voronoi((TexCoords+(noise*0.05))*int(50*mix(5, 0.1, smoothstep(50, 250, 20*int(realDist/20) ))));

			float rockPalette = rockFactors.y;
			vec3 color = palette( // Make this a curl noise?
				random(rockBoundary.xy),
				vec3(0.5, 0.5, 0.5), vec3(0.5, 0.5, 0.5),
				mix(vec3(1.0, 1.0, 0.50), vec3(1.0, 1.0, 1.0), rockPalette),
				mix(vec3(0.80, 0.90, 0.30), vec3(0.30, 0.20, 0.20), rockPalette)
			);

			vec3 rockColor = color * ((1-smoothstep(0.0, max(0.75, random(rockBoundary.xy)), rockBoundary.z)) * 0.8 + 0.2);

			mat.albedo = mix(mat.albedo, rockColor, smoothstep(0.5, 1, rockFactor));
			mat.roughness = mix(0.7, 0.1, wetness*smoothstep(0.01, 0.02, rockBoundary.z ));
			mat.normalScale = 40.0;
			mat.normalStrength = mix(0.1, 0.05, wetness);
		}
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
TerrainMaterial getCliffMaterial(float height, float moisture, float noise) {
	TerrainMaterial mat;
	float h = height + noise * 5.0;

	// Low altitude cliffs: brown/dark rock (often wet)
	if (h < HEIGHT_FOREST_END) {
		float wetness = 0.3 + noise * 0.2;
		vec3 baseColor = mix(COL_ROCK_BROWN, COL_ROCK_DARK, wetness);
		mat = getRockTexture(baseColor, height, moisture, noise);
		mat.metallic = 0.0;
		return mat;
	}

	// Mid altitude: mixed brown/grey
	if (h < HEIGHT_SNOW_START) {
		float t = smoothstep(HEIGHT_FOREST_END, HEIGHT_SNOW_START, h);
		vec3 baseColor = mix(COL_ROCK_BROWN, COL_ROCK_GREY, t + noise * 0.2);
		mat = getRockTexture(baseColor, height, moisture, noise);
		mat.metallic = 0.0;
		return mat;
	}

	// High altitude cliffs: grey rock with snow patches
	float snowPatch = smoothstep(HEIGHT_SNOW_START, HEIGHT_PEAK, h) * 0.4;
	vec3  highRock = mix(COL_ROCK_GREY, COL_ROCK_DARK, noise * 0.3);
	vec3 baseColor = mix(highRock, COL_SNOW_OLD, snowPatch);
	mat = getRockTexture(baseColor, height, moisture, noise);
	mat.metallic = 0.0;
	return mat;
}

void processWaterLayer(vec3 norm, float dist, float fade) {
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
	float primaryShadow;
	vec3 lighting = apply_lighting_pbr(FragPos, norm, surfaceColor, 0.05, 0.9, 1.0, primaryShadow).rgb;
	vec3 final_color = lighting + grid_color;

	// Distance fade and distant cyan blend (matching terrain style)
	vec4 baseColor = vec4(final_color, fade);
	FragColor = mix(vec4(0.0, 0.7, 0.7, baseColor.a) * length(baseColor), baseColor, step(1.0, fade));

	// Output view-space normal
	NormalOut = vec4(normalize(mat3(view) * norm), primaryShadow);
	AlbedoOut = vec4(surfaceColor, 1.0);
	return;
}

TerrainMaterial calculateMaterial(float largeNoise, float slope) {
	TerrainMaterial finalMaterial;
	// ========================================================================
	// Material Calculation
	// ========================================================================

	// Height with noise distortion for natural boundaries
	float baseHeight = FragPos.y;
	float distortedHeight = baseHeight + largeNoise * 5.0 * worldScale;

	// Slope analysis: 1.0 = flat horizontal, 0.0 = vertical cliff
	float distortedSlope = slope + largeNoise * 0.08;

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
	cliffMask += (largeNoise - 0.5) * 0.15;
	cliffMask = clamp(cliffMask, 0.0, 1.0);

	// Don't make beach areas into cliffs
	float beachMask = 1.0 - smoothstep(0.0, HEIGHT_BEACH_END + 2.0, baseHeight);
	cliffMask *= (1.0 - beachMask);

	TerrainMaterial biomeMat = getBiomeMaterial(distortedHeight, moisture, largeNoise);
	TerrainMaterial cliffMat = getCliffMaterial(baseHeight, moisture, largeNoise);

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
	finalMaterial.albedo *= (1.0 + largeNoise * 0.12);
	return finalMaterial;
}

TerrainMaterial processGrass(float largeNoise, vec3 norm, float realDist, float baseFreq, float freezingScale, float dist, float n_fade, TerrainMaterial finalMaterial) {
	TerrainMaterial grassMaterial;
	// ========================================================================
	// Grass-based Tinting and AO baseline shift
	// ========================================================================
	float grassAO = 0.0;
	vec3 perturbedNorm = norm;
	float stepDist = 50*int(realDist/50);
	float freqScale = mix(1.0, 0.25, smoothstep(150.0, 160.0, stepDist + 50.0 * largeNoise));
	freqScale = mix(5.0, freqScale, smoothstep(45, 50, stepDist));

	float blueNoise = fastBlueNoise(FragPos.xz * (baseFreq * 0.05 * freqScale), 0) * 0.5 + 0.5;
	float blueNoiseA = fastBlueNoise(FragPos.xz * (baseFreq * 0.1 * freqScale), 1) * 0.5 + 0.5;
	if (u_grassGlobal.enabled != 0 && freezingScale == 0) {

		vec2  biomeUV = (TexCoords * uRawChunkSize + 0.5) / (uRawChunkSize + 1.0);
		vec4  biomeData = texture(uBiomeMap, vec3(biomeUV, TextureSlice));

		// Baked biome override
		vec4 bakedDispGrass = texture(u_displacementArray, vec3(biomeUV, TextureSlice));
		if (bakedDispGrass.a > 0.0) {
			biomeData.x = bakedDispGrass.a;
			biomeData.y = 0.0; // Full override
		}
		int   idxA = int(biomeData.r * 255.0 + 0.5);
		int   idxB = min(idxA + 1, 7);
		float t = biomeData.g;

		float densityA = u_grassBiomes[idxA].density * float(u_grassBiomes[idxA].enabled);
		float densityB = u_grassBiomes[idxB].density * float(u_grassBiomes[idxB].enabled);
		float interpolatedDensity = mix(densityA, densityB, t) * u_grassGlobal.densityMultiplier;

		vec3 colorA = u_grassBiomes[idxA].colorBottom.rgb;
		vec3 colorB = u_grassBiomes[idxB].colorBottom.rgb;
		vec3 grassColor = mix(colorA, colorB, smoothstep(0, blueNoiseA, t));

		float rigidA = u_grassBiomes[idxA].rigidity;
		float rigidB = u_grassBiomes[idxB].rigidity;
		float rigidity = clamp(mix(rigidA, rigidB, step(blueNoise, t)) * u_grassGlobal.rigidityMultiplier, 0, 1);

		// Apply effect only on relatively flat surfaces where grass would grow
		float grassMask = smoothstep(0.7, 0.8, norm.y) * clamp(interpolatedDensity, 0.0, 1.0);

		// AO baseline shift - darken dense grass areas
		grassAO = grassMask * 0.75;

		float distanceFactor = smoothstep(200, 350, dist);

		perturbedNorm = mix(norm, vec3(0.0, 1.0, 0.0), interpolatedDensity * distanceFactor);
		perturbedNorm = normalize(perturbedNorm);

		float windAtPos = FragPos.x*sin(n_fade)+FragPos.z*cos(n_fade);
		float windThreshold = rigidity * 2.0;
		float effectiveWindStrength = max(0.0, length(windAtPos) - windThreshold);

		vec3 undersideColor = grassColor * 1.25 + vec3(0.05, 0.05, 0.0);
		vec3 dynamicGrassColor = mix(grassColor, undersideColor, smoothstep(0, blueNoise, effectiveWindStrength));

		finalMaterial.albedo = mix(finalMaterial.albedo, dynamicGrassColor, smoothstep(0, blueNoise, grassMask));

		float floorTexture = pow(fastRidge3d(FragPos * 0.01 * freqScale) * 0.5 + 0.5, 2);

		floorTexture = mix(1.0, floorTexture, (1.0-smoothstep(0, 150, dist)));
		float albedoMultiplier = floorTexture;

		finalMaterial.albedo *= albedoMultiplier;

		// vec2 wore = fastWorley3dID( fastCurl3d( FragPos*0.005));
		// vec3 color = palette( // Make this a curl noise?
		// 	wore.y,
		// 	vec3(0.5, 0.5, 0.5), vec3(0.5, 0.5, 0.5),
		// 	mix(vec3(1.0, 1.0, 0.50), vec3(1.0, 1.0, 1.0), sin(wore.y)),
		// 	mix(vec3(0.80, 0.90, 0.30), vec3(0.30, 0.20, 0.20), sin(wore.y))
		// );

		// finalMaterial.albedo = mix(finalMaterial.albedo, color, wore.x);

/*
		// Select flower color from a vibrant palette based on blue noise and position
		vec3 flowerColor;
		float colorSelector = fract(blueNoiseA * 3.0 + length(FragPos.xz) * 0.01);
		if (colorSelector < 0.3) {
			flowerColor = vec3(1.0, 0.2, 0.4); // Pinkish
		} else if (colorSelector < 0.6) {
			flowerColor = vec3(1.0, 0.8, 0.1); // Yellow/Orange
		} else {
			flowerColor = vec3(0.5, 0.2, 1.0); // Purple
		}

		// Occasional white flowers
		if (fastSimplex3d(FragPos * 0.01) > 0.8) flowerColor = vec3(1.0, 1.0, 1.0);

		float flowerScale = mix(mix(0.75, 0.35, smoothstep(50.0, 100.0, realDist)), 0.01, smoothstep(100, 150, realDist));
		float flowerMask = smoothstep(0.5, 0.7, grassMask) * smoothstep(flowerScale, flowerScale + 0.10, worley) * smoothstep(0.6, 0.95, max(fastWorley3d(FragPos/50.0), pow(fastRidge3d(FragPos/200.0), 3)));
		finalMaterial.albedo = mix(finalMaterial.albedo, flowerColor, flowerMask);
*/

		// finalMaterial.roughness = mix(finalMaterial.roughness, clamp(finalMaterial.roughness * dynamicBlend, 0.0, 1.0), distanceFactor);
		finalMaterial.roughness = mix(finalMaterial.roughness, clamp(finalMaterial.roughness, 0.0, 1.0), distanceFactor);
	}
	return grassMaterial = finalMaterial;
}

void generateNoise();
void processGrain();
void processGridFade();


void main() {
	if (uIsShadowPass) {
		// Output only depth (handled by hardware)
		return;
	}
	// vec2  biomeUV = (TexCoords * uRawChunkSize + 0.5) / (uRawChunkSize + 1.0);
	// vec4  biomeData = texture(uBiomeMap, vec3(biomeUV, TextureSlice));
	// vec4 bakedDispGrass = texture(u_displacementArray, vec3(biomeUV, TextureSlice));
	// FragColor = bakedDispGrass;
	// FragColor = vec4(smoothstep(0, 2, tessFactor), smoothstep(2, 4, tessFactor), smoothstep(4, 8, tessFactor), 1.0);
	// return;



	vec3  norm = normalize(Normal);
	float slope = dot(norm, vec3(0.0, 1.0, 0.0));
	vec3  scaledFragPos = FragPos / worldScale;

	float dist = length(FragPos.xz - viewPos.xz);
	float realDist = distance(FragPos, viewPos);

	float baseFreq = 0.1 / worldScale;
	float stepDist = 50*int(realDist/50);
	float distanceFactor = dist * smoothstep(0, 10.0, FragPos.y);
	float freqScale = mix(1.0, 0.25, smoothstep(150.0, 160.0, stepDist + 50.0));
	freqScale = mix(5.0, freqScale, smoothstep(45, 50, stepDist));


	// ========================================================================
	// Noise Generation
	// ========================================================================
	// Scale world-space position for detail noise to match terrain scaling
	// float largeNoise =    mix(fastFbm3d(FragPos * (baseFreq * 5.0)), fastWarpedFbm3d(FragPos * (baseFreq * 0.5)),
	// fastWorley3d(FragPos * (baseFreq * 0.1)));
	float largeNoise = fastWarpedFbm3d(FragPos * (baseFreq * 0.1));
	float n_fade = fastSimplex3d(vec3(FragPos.xz / (250 * worldScale), time * 0.09));
	float blueNoise = fastBlueNoise(FragPos.xz * (baseFreq * 0.05 * freqScale), 0) * 0.5 + 0.5;
	float blueNoiseA = fastBlueNoise(FragPos.xz * (baseFreq * 0.1 * freqScale), 1) * 0.5 + 0.5;


	float fade_start = 560.0 * worldScale;
	float fade_end = 570.0 * worldScale;
	float fade = 1.0 - smoothstep(fade_start, fade_end, dist + n_fade * 40.0);
	float noseFade = fade_start - 100.0;

	if (fade < 0.2) {
		discard;
		// FragColor = vec4(0);
	}

	if (vIsWater > 0.5) {
		processWaterLayer(norm, dist, fade);
		return;
	}

	TerrainMaterial finalMaterial = calculateMaterial(largeNoise, slope);

	float freezingScale = 1.0 - smoothstep(255.372, 273.15, temperature);

	// Apply global wetness from precipitation
	// Wet surfaces are darker and much smoother (glossier)
	float globalWetness = max(wetness, freezingScale);
	finalMaterial.albedo = mix(finalMaterial.albedo, finalMaterial.albedo * 0.5, globalWetness * 0.5);
	finalMaterial.roughness = mix(finalMaterial.roughness, 0.1, globalWetness * 0.8);

	float waterEffect = 0.0;
	// Running water effect on rock surfaces
	if (wetness > 0.6 && freezingScale < 0.1) {
		float rockSurface = 1.0 - smoothstep(0.2, 0.5, slope); // Steeper is rockier
		rockSurface = max(rockSurface, smoothstep(0.2, -0.6, vSubstrate));
		float waterFlowMask = rockSurface * smoothstep(0.6, 0.9, wetness);

		if (waterFlowMask > 0.01) {
			// // Scrolling procedural flow
			// vec3 flowOffset = vec3(0, time * 2.0, 0);
			// vec3 p_flow = (FragPos + flowOffset + time * norm * vec3(0.25, -1, -0.25)) * 1.5;
			// vec3 flowNoise = fastCurl3d(p_flow * 0.08);

			vec3 surfaceDown = vec3(0.0, -1.0, 0.0) - dot(vec3(0.0, -1.0, 0.0), norm) * norm;
			vec3 flowDir = normalize(surfaceDown + vec3(0.00001, 0.0, 0.0));
			float flowSpeed = 2.0;
			vec3 p_flow = (FragPos + -flowDir * time * flowSpeed) * 1.5;
			vec3 flowNoise = fastCurl3d(p_flow * 0.08);

			// Create animated streaks
			float streaks = smoothstep(0.3, 0.8, abs(flowNoise.x));
			streaks *= smoothstep(0.4, 0.6, fract(flowNoise.y * 0.5 + time * 0.8));

			waterEffect = waterFlowMask * streaks;
			finalMaterial.albedo = mix(finalMaterial.albedo, finalMaterial.albedo * 0.5, waterEffect * 0.5);
			finalMaterial.roughness = mix(finalMaterial.roughness, 0.00, waterEffect);
			finalMaterial.metallic = mix(finalMaterial.metallic, 0.1, waterEffect);

			// Perturb normals for the water flow
			if (waterEffect > 0.05) {
				vec3 flowNorm = normalize(flowNoise * 2.0 - 1.0);
				norm = normalize(mix(norm, flowNorm, waterEffect*0.8));
			}
		}
	}

	// Extra variety for rocky/steep areas to complement normals
	float rockyVar = largeNoise;
	float rockyMask = smoothstep(0.5, 0.2, slope); // More variety on steeper slopes
	finalMaterial.albedo = mix(finalMaterial.albedo, finalMaterial.albedo * (1.0 + rockyVar * 0.2), rockyMask);

	// ========================================================================
	// Advanced Erosion Filter Coloration
	// ========================================================================
	// Apply the extracted color mapping using the data passed from the tessellation shader
	finalMaterial.albedo = applyErosionColorMappingDefault(finalMaterial.albedo, vRidgeMap, vErosionDelta);

	// ========================================================================
	// Grass-based Tinting and AO baseline shift
	// ========================================================================
	float grassAO = 0.0;
	vec3 perturbedNorm = norm;
	if (u_grassGlobal.enabled != 0 && freezingScale == 0) {

		vec2  biomeUV = (TexCoords * uRawChunkSize + 0.5) / (uRawChunkSize + 1.0);
		vec4  biomeData = texture(uBiomeMap, vec3(biomeUV, TextureSlice));

		// Baked biome override
		vec4 bakedDispGrass = texture(u_displacementArray, vec3(biomeUV, TextureSlice));
		if (bakedDispGrass.a > 0.0) {
			biomeData.x = bakedDispGrass.a;
			biomeData.y = 0.0; // Full override
		}
		int   idxA = int(biomeData.r * 255.0 + 0.5);
		int   idxB = min(idxA + 1, 7);
		float t = biomeData.g;

		float densityA = u_grassBiomes[idxA].density * float(u_grassBiomes[idxA].enabled);
		float densityB = u_grassBiomes[idxB].density * float(u_grassBiomes[idxB].enabled);
		float interpolatedDensity = mix(densityA, densityB, t) * u_grassGlobal.densityMultiplier;

		vec3 colorA = u_grassBiomes[idxA].colorBottom.rgb;
		vec3 colorB = u_grassBiomes[idxB].colorBottom.rgb;
		vec3 grassColor = mix(colorA, colorB, smoothstep(0, blueNoiseA, t));

		float rigidA = u_grassBiomes[idxA].rigidity;
		float rigidB = u_grassBiomes[idxB].rigidity;
		float rigidity = clamp(mix(rigidA, rigidB, step(blueNoise, t)) * u_grassGlobal.rigidityMultiplier, 0, 1);

		// Apply effect only on relatively flat surfaces where grass would grow
		float grassMask = smoothstep(0.7, 0.8, norm.y) * clamp(interpolatedDensity, 0.0, 1.0);

		// AO baseline shift - darken dense grass areas
		grassAO = grassMask * 0.75;

		float distanceFactor = smoothstep(200, 350, dist);

		perturbedNorm = mix(norm, vec3(0.0, 1.0, 0.0), interpolatedDensity * distanceFactor);
		perturbedNorm = normalize(perturbedNorm);

		float windAtPos = FragPos.x*sin(n_fade)+FragPos.z*cos(n_fade);
		float windThreshold = rigidity * 2.0;
		float effectiveWindStrength = max(0.0, length(windAtPos) - windThreshold);

		vec3 undersideColor = grassColor * 1.25 + vec3(0.05, 0.05, 0.0);
		vec3 dynamicGrassColor = mix(grassColor, undersideColor, smoothstep(0, blueNoise, effectiveWindStrength));

		finalMaterial.albedo = mix(finalMaterial.albedo, dynamicGrassColor, smoothstep(0, blueNoise, grassMask));

		float floorTexture = pow(fastRidge3d(FragPos * 0.01 * freqScale) * 0.5 + 0.5, 2);

		floorTexture = mix(1.0, floorTexture, (1.0-smoothstep(0, 150, dist)));
		float albedoMultiplier = floorTexture;

		finalMaterial.albedo *= albedoMultiplier;

		// vec2 wore = fastWorley3dID( fastCurl3d( FragPos*0.005));
		// vec3 color = palette( // Make this a curl noise?
		// 	wore.y,
		// 	vec3(0.5, 0.5, 0.5), vec3(0.5, 0.5, 0.5),
		// 	mix(vec3(1.0, 1.0, 0.50), vec3(1.0, 1.0, 1.0), sin(wore.y)),
		// 	mix(vec3(0.80, 0.90, 0.30), vec3(0.30, 0.20, 0.20), sin(wore.y))
		// );

		// finalMaterial.albedo = mix(finalMaterial.albedo, color, wore.x);

/*
		// Select flower color from a vibrant palette based on blue noise and position
		vec3 flowerColor;
		float colorSelector = fract(blueNoiseA * 3.0 + length(FragPos.xz) * 0.01);
		if (colorSelector < 0.3) {
			flowerColor = vec3(1.0, 0.2, 0.4); // Pinkish
		} else if (colorSelector < 0.6) {
			flowerColor = vec3(1.0, 0.8, 0.1); // Yellow/Orange
		} else {
			flowerColor = vec3(0.5, 0.2, 1.0); // Purple
		}

		// Occasional white flowers
		if (fastSimplex3d(FragPos * 0.01) > 0.8) flowerColor = vec3(1.0, 1.0, 1.0);

		float flowerScale = mix(mix(0.75, 0.35, smoothstep(50.0, 100.0, realDist)), 0.01, smoothstep(100, 150, realDist));
		float flowerMask = smoothstep(0.5, 0.7, grassMask) * smoothstep(flowerScale, flowerScale + 0.10, worley) * smoothstep(0.6, 0.95, max(fastWorley3d(FragPos/50.0), pow(fastRidge3d(FragPos/200.0), 3)));
		finalMaterial.albedo = mix(finalMaterial.albedo, flowerColor, flowerMask);
*/

		// finalMaterial.roughness = mix(finalMaterial.roughness, clamp(finalMaterial.roughness * dynamicBlend, 0.0, 1.0), distanceFactor);
		finalMaterial.roughness = mix(finalMaterial.roughness, clamp(finalMaterial.roughness, 0.0, 1.0), distanceFactor);
	}

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

	if (perturbFactor >= 0.1 && normalStrength > 0.0 && (dist + 50.0 * largeNoise) < 200 && waterEffect == 0) {
		float roughnessStrength = smoothstep(0.1, 1.0, perturbFactor) * normalStrength;
		float roughnessScale = normalScale * 0.05;
		vec3  scaledFragPos = FragPos / worldScale;

		// Sample biome noise type (using unused params.w)
		vec2  biomeUV = (TexCoords * uRawChunkSize + 0.5) / (uRawChunkSize + 1.0);
		vec4  biomeInfo = texture(uBiomeMap, vec3(biomeUV, TextureSlice));

		// Baked biome override
		vec4 bakedDisp = texture(u_displacementArray, vec3(biomeUV, TextureSlice));
		if (bakedDisp.a > 0.0) {
			biomeInfo.x = bakedDisp.a;
			biomeInfo.y = 0.0; // Full override
		}

		float noiseTypeA = u_biomes[int(biomeInfo.x * 255.0 + 0.5)].params.w;
		float noiseTypeB = u_biomes[min(int(biomeInfo.x) + 1, 7)].params.w;
		float noiseType = mix(noiseTypeA, noiseTypeB, biomeInfo.y);

		// Use finite difference to approximate the gradient of the noise field
		float eps = 0.015;
		float n, nx, nz;

		if (freezingScale < 0.5) {
			n = fastRidge3d(0.1 * scaledFragPos * roughnessScale);
			nx = fastRidge3d(0.1 * (scaledFragPos + vec3(eps, 0.0, 0.0)) * roughnessScale);
			nz = fastRidge3d(0.1 * (scaledFragPos + vec3(0.0, 0.0, eps)) * roughnessScale);
		}
		else {
			n = fastWarpedFbm3d(0.1 * scaledFragPos * roughnessScale);
			nx = fastWarpedFbm3d(0.1 * (scaledFragPos + vec3(eps, 0.0, 0.0)) * roughnessScale);
			nz = fastWarpedFbm3d(0.1 * (scaledFragPos + vec3(0.0, 0.0, eps)) * roughnessScale);
		}

		// Compute local tangent space to orient the perturbation.
		// Using a stable basis that doesn't flip at Z-axis alignment.
		vec3 v = abs(perturbedNorm.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
		vec3 tangent = normalize(cross(perturbedNorm, v));
		vec3 bitangent = cross(tangent, perturbedNorm);

		// Apply perturbation based on noise gradient
		vec3 perturbation = (tangent * (n - nx) + bitangent * (n - nz)) * (roughnessStrength / eps);
		perturbedNorm = normalize(perturbedNorm + perturbation);

		// Toksvig-like Adjustment: Increase roughness based on normal variance
		// Procedural normals can cause aliasing; we compensate by increasing roughness
		// where the normal gradient is high.
		float variance = dot(perturbation, perturbation);
		roughness = sqrt(clamp(roughness * roughness + variance * 0.25, 0.0, 1.0));
	}

	if (freezingScale > 0) {
		vec3 snowColor = vec3(0.9, 0.95, 1.0+0.01*grassAO);

		albedo = mix(albedo, snowColor, freezingScale);
		roughness = mix(roughness, 0.85, freezingScale);
		metallic = mix(metallic, 0.0, freezingScale);
	}

	float primaryShadow;
	vec3 lighting = apply_lighting_pbr(FragPos, perturbedNorm, albedo, roughness, metallic, 1.0 - grassAO, primaryShadow).rgb;
	lighting.b *= 1 + (0.2 * freezingScale * (1-primaryShadow));
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

	// Output view-space normal
	NormalOut = vec4(normalize(mat3(view) * perturbedNorm), primaryShadow);
	AlbedoOut = vec4(albedo, 1.0);

	// Calculate screen-space velocity and material properties
	vec2 a = (CurPosition.xy / CurPosition.w) * 0.5 + 0.5;
	vec2 b = (PrevPosition.xy / PrevPosition.w) * 0.5 + 0.5;
	Velocity = vec4(a - b, roughness, metallic);
}
