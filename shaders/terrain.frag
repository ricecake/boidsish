#version 430 core
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

#include "helpers/fast_noise.glsl"
#include "helpers/lighting.glsl"
#include "helpers/terrain_noise.glsl"
// #include "helpers/noise.glsl"

uniform bool uIsShadowPass = false;

// Biome texture array: RG8 - R=low_idx, G=t
uniform sampler2DArray uBiomeMap;

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

	// ========================================================================
	// Noise Generation
	// ========================================================================
	// Scale world-space position for detail noise to match terrain scaling
	float baseFreq = 0.1 / worldScale;
	float largeNoise = fastWarpedFbm3d(FragPos * (baseFreq * 0.5));
	float medNoise = fastWorley3d(vec3(largeNoise) * (baseFreq * 2.0));
	float fineNoise = fastFbm3d(FragPos * (baseFreq * 5.0));

	float combinedNoise = largeNoise * 0.6 + (1.0 - medNoise) * 0.3 + fineNoise * 0.1;

	// Distance Fade -- precalc
	vec3 norm = normalize(Normal);

	if (norm == vec3(0, 0, 0)) {
		norm = normalize(vec3(largeNoise, 1, largeNoise));
	}

	float dist = length(FragPos.xz - viewPos.xz);
	float n_fade = snoise(vec3(FragPos.xy / (25 * worldScale), time * 0.08));
	float fade_start = 560.0 * worldScale;
	float fade_end = 570.0 * worldScale;
	float fade = 1.0 - smoothstep(fade_start, fade_end, dist + n_fade * 40.0);

	if (fade < 0.2) {
		discard;
	}

	// Height with noise distortion for natural boundaries
	float baseHeight = FragPos.y;
	float distortedHeight = baseHeight + largeNoise * 5.0 * worldScale;

	// Slope analysis: 1.0 = flat horizontal, 0.0 = vertical cliff
	float slope = dot(norm, vec3(0.0, 1.0, 0.0));
	float distortedSlope = slope + medNoise * 0.08;

	// ========================================================================
	// Material Calculation (Unified Biome Map)
	// ========================================================================

	// Get biome data from texture
	vec2  biomeData = texture(uBiomeMap, vec3(TexCoords, TextureSlice)).rg;
	int   lowIdx = int(biomeData.r * 255.0 + 0.5);
	int   highIdx = min(lowIdx + 1, 7);
	float t = biomeData.g;

	// Organic biome transitions using warped noise
	float warpScale = 0.05 / worldScale;
	float transitionWarp = fastWarpedFbm3d(FragPos * warpScale);
	float organicT = clamp(t + transitionWarp * 0.4 - 0.2, 0.0, 1.0);
	organicT = smoothstep(0.0, 1.0, organicT);

	BiomeProperties lowBiome = biomes[lowIdx];
	BiomeProperties highBiome = biomes[highIdx];

	vec3  albedo = mix(lowBiome.albedo_roughness.rgb, highBiome.albedo_roughness.rgb, organicT);
	float roughness = mix(lowBiome.albedo_roughness.a, highBiome.albedo_roughness.a, organicT);
	float metallic = mix(lowBiome.params.x, highBiome.params.x, organicT);

	// // Slope-based cliff blending
	float verticalMask = smoothstep(0.4, 0.2, slope);

	// Valley/ridge detection
	float valleyFactor = calculateValleyFactor(FragPos);

	// Moisture calculation
	float moisture = calculateMoisture(baseHeight, valleyFactor, FragPos);

	// Valley lushness boost
	float valleyLushness = clamp(-valleyFactor, 0.0, 1.0);
	moisture = mix(moisture, min(moisture + 0.3, 1.0), valleyLushness);

	// ========================================================================
	// Detail Variation
	// ========================================================================

	// ========================================================================
	// Material Calculation
	// ========================================================================

	// Get base biome material
	TerrainMaterial biomeMat = getBiomeMaterial(distortedHeight, moisture, combinedNoise);

	// Get cliff material
	TerrainMaterial cliffMat = getCliffMaterial(baseHeight, medNoise);

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

	// Blend biome with cliff material
	TerrainMaterial finalMaterial;
	finalMaterial.albedo = mix(biomeMat.albedo, cliffMat.albedo, cliffMask);
	finalMaterial.roughness = mix(biomeMat.roughness, cliffMat.roughness, cliffMask);
	finalMaterial.metallic = mix(biomeMat.metallic, cliffMat.metallic, cliffMask);
	finalMaterial.normalScale = mix(biomeMat.normalScale, cliffMat.normalScale, cliffMask);
	finalMaterial.normalStrength = mix(biomeMat.normalStrength, cliffMat.normalStrength, cliffMask);

	albedo = finalMaterial.albedo;
	roughness = finalMaterial.roughness;
	metallic = finalMaterial.metallic;

	// ========================================================================
	// Detail Variation
	// ========================================================================

	// Large-scale macro color shifts
	float macroNoise = fastSimplex3d(FragPos * (baseFreq * 0.1));
	albedo *= (1.0 + macroNoise * 0.12);

	// Add subtle color variation based on combined noise
	albedo *= (1.0 + combinedNoise * 0.15);

	// Extra variety for rocky/steep areas to complement normals
	float rockyVar = fastWarpedFbm3d(FragPos * (baseFreq * 4.0));
	float rockyMask = smoothstep(0.5, 0.2, slope); // More variety on steeper slopes
	albedo = mix(albedo, albedo * (1.0 + rockyVar * 0.2), rockyMask);

	// ========================================================================
	// Normal Perturbation (Grain)
	// ========================================================================
	vec3  perturbedNorm = norm;
	float normalStrength = mix(biomes[lowIdx].params.y, biomes[highIdx].params.y, t);
	float normalScale = mix(biomes[lowIdx].params.z, biomes[highIdx].params.z, t);

	normalStrength = finalMaterial.normalStrength;
	normalScale = finalMaterial.normalScale;

	if (perturbFactor >= 0.1 && normalStrength > 0.0) {
		float roughnessStrength = smoothstep(0.1, 1.0, perturbFactor) * normalStrength;
		float roughnessScale = normalScale * 0.05;
		vec3  scaledFragPos = FragPos / worldScale;

		// Use finite difference to approximate the gradient of the noise field
		float eps = 0.015;
		float n = fastWorley3d(0.5 * scaledFragPos * roughnessScale);
		float nx = fastWorley3d(0.5 * (scaledFragPos + vec3(eps, 0.0, 0.0)) * roughnessScale);
		float nz = fastWorley3d(0.5 * (scaledFragPos + vec3(0.0, 0.0, eps)) * roughnessScale);

		// Compute local tangent space to orient the perturbation
		vec3 tangent = normalize(cross(norm, vec3(0, 0, 1)));
		if (abs(norm.z) > 0.9)
			tangent = normalize(cross(norm, vec3(1, 0, 0)));
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
	vec3 lighting = apply_lighting_pbr(FragPos, perturbedNorm, albedo, roughness, metallic, 1.0).rgb;

	// ========================================================================
	// Neon 80s Synth Style (Night Theme)
	// ========================================================================
	if (nightFactor > 0.0) {
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
		vec3 newLighting = mix(lighting, lighting * vec3(0.4, 0.1, 0.5), nightFactor * 0.7);

		// Add cyan grid with magenta glow
		newLighting += gridLine * cyan * nightFactor * 0.8;
		newLighting += gridGlowFactor * magenta * nightFactor * 0.4;

		// Height-based neon pulse/glow
		float heightGlow = smoothstep(0.0, 100.0 * worldScale, FragPos.y);
		newLighting += magenta * heightGlow * nightFactor * (0.8 + 0.2 * sin(time * 0.5));

		float nightNoise = fastWorley3d(vec3(FragPos.xy / (25 * worldScale), time * 0.08));
		float nightFade = smoothstep(fade_start - 10, fade_end, dist + nightNoise * 100.0);
		lighting = mix(lighting, newLighting, nightFade);
	}

	// ========================================================================
	// Distance Fade
	// ========================================================================
	// The AtmosphereEffect will handle the scattering over the terrain.
	// We just handle the alpha fade for transition to the skybox.
	FragColor = vec4(lighting, mix(0.0, fade, step(0.01, FragPos.y)));
}
