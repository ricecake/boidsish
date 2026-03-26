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
in float      vIsWater;

#include "helpers/fast_noise.glsl"
#include "helpers/lighting.glsl"
#include "helpers/terrain_noise.glsl"
#include "visual_effects.glsl"
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

float sampleProceduralHeightMap(vec2 uv, vec3 biomeWeights1, vec3 biomeWeights2, float windIntensity) {
	// Base noises at requested lower frequencies (division instead of multiplication)
	float worleyDetail = 1.0 - fastWorley3d(vec3(uv / 18.0, 0.0));
	float worleyClump = 1.0 - fastWorley3d(vec3(uv / 4.0, 0.0));
	float simplexRipple = fastSimplex3d(vec3(uv / 10.0, 0.0)) * 0.5 + 0.5;
	float ridgeRock = fastRidge3d(vec3(uv / 8.0, 0.0));

	// Biome-specific height logic
	// biomeWeights1: x=Sand, y=LushGrass, z=DryGrass
	// biomeWeights2: x=Forest, y=AlpineMeadow, z=Rock

	float grassHeight = mix(worleyDetail, worleyDetail * worleyClump, 0.5);
	float sandHeight = simplexRipple * 0.3 + worleyDetail * 0.1;
	float rockHeight = ridgeRock * 0.6 + worleyDetail * 0.2;
	float snowHeight = simplexRipple * 0.2 + worleyDetail * 0.1;

	float h = 0.0;
	h += (biomeWeights1.x) * sandHeight;               // Sand
	h += (biomeWeights1.y + biomeWeights1.z) * grassHeight; // Grasses
	h += (biomeWeights2.x + biomeWeights2.y) * grassHeight; // Forest/Alpine
	h += (biomeWeights2.z) * rockHeight;               // Rock (Brown/Grey)
	h += (1.0 - clamp(dot(biomeWeights1, vec3(1)) + dot(biomeWeights2, vec3(1)), 0, 1)) * snowHeight; // Snow

	return clamp(h, 0.0, 1.0);
}

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

	float dist = length(FragPos.xz - viewPos.xz);
	// float n_fade = snoise(vec3(FragPos.xy / (25 * worldScale), time * 0.08));
	float n_fade = fastSimplex3d(vec3(FragPos.xy / (125 * worldScale), time * 0.09));
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
	float largeNoise = sin(length(FragPos.xz) * 100);
	float medNoise = sin(length(FragPos.xz) * 2000);
	float fineNoise = sin(length(FragPos.xz) * 3000);
	float macroNoise = sin(length(FragPos.xz) * 44323);
	float combinedNoise = sin(length(FragPos.xz) * 5222343);

	float distanceFactor = dist * smoothstep(0, 10.0, FragPos.y);
	float noseFade = fade_start - 100.0;

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

	// Sample Biome Map
	vec2  remappedUV = (TexCoords * 32.0 + 0.5) / 33.0; // matching placement.comp
	vec2  biomeData = textureLod(uBiomeMap, vec3(remappedUV, TextureSlice), 0.0).rg;
	int   lowIdx = int(biomeData.r * 255.0 + 0.5);
	int   highIdx = min(lowIdx + 1, 7);
	float biomeT = biomeData.g;

	vec3 bWeights1 = vec3(0.0); // Sand, LushGrass, DryGrass
	vec3 bWeights2 = vec3(0.0); // Forest, AlpineMeadow, Rock

	if (lowIdx == 0)
		bWeights1.x += (1.0 - biomeT);
	else if (lowIdx == 1)
		bWeights1.y += (1.0 - biomeT);
	else if (lowIdx == 2)
		bWeights1.z += (1.0 - biomeT);
	else if (lowIdx == 3)
		bWeights2.x += (1.0 - biomeT);
	else if (lowIdx == 4)
		bWeights2.y += (1.0 - biomeT);
	else if (lowIdx == 5 || lowIdx == 6)
		bWeights2.z += (1.0 - biomeT);

	if (highIdx == 0)
		bWeights1.x += biomeT;
	else if (highIdx == 1)
		bWeights1.y += biomeT;
	else if (highIdx == 2)
		bWeights1.z += biomeT;
	else if (highIdx == 3)
		bWeights2.x += biomeT;
	else if (highIdx == 4)
		bWeights2.y += biomeT;
	else if (highIdx == 5 || highIdx == 6)
		bWeights2.z += biomeT;

	TerrainMaterial biomeMat = getBiomeMaterial(distortedHeight, moisture, combinedNoise);
	TerrainMaterial cliffMat = getCliffMaterial(baseHeight, medNoise);

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

	vec3  albedo = finalMaterial.albedo;
	float roughness = finalMaterial.roughness;
	float metallic = finalMaterial.metallic;
	float normalStrength = finalMaterial.normalStrength;
	float normalScale = finalMaterial.normalScale;

	// ========================================================================
	// Detail Variation & Wind
	// ========================================================================
	float fateFactor = fastWorley3d(vec3(FragPos.xz / 50.0, time * 0.25)) * 0.5 + 0.50;
	vec3  windForce = fastCurl3d(
		vec3(FragPos.x * 0.0005 + time * 0.00125, FragPos.y * 0.001, FragPos.z * 0.0005 + time * 0.0125)
	);
	vec3 rawWindNudge = (fateFactor * windForce);

	// Compute local tangent space for POM and Normal Perturbation
	vec3 v_ref = abs(norm.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
	vec3 tangent = normalize(cross(v_ref, norm));
	vec3 bitangent = cross(norm, tangent);

	// ========================================================================
	// Procedural Surface Detail POM
	// ========================================================================
	float pomShadow = 1.0;
	float grassFactor = (bWeights1.y + bWeights1.z + bWeights2.x + bWeights2.y);
	float sandFactor = bWeights1.x;
	float rockFactor = bWeights2.z;
	float snowFactor = (1.0 - clamp(dot(bWeights1, vec3(1)) + dot(bWeights2, vec3(1)), 0, 1));

	// Determine height scale for POM based on biome
	float detailHeightScale = 0.0;
	detailHeightScale += grassFactor * 0.35;
	detailHeightScale += sandFactor * 0.05;
	detailHeightScale += rockFactor * 0.15;
	detailHeightScale += snowFactor * 0.08;

	if (detailHeightScale > 0.01 && dist < 120.0) {
		vec3  V = normalize(viewPos - FragPos);
		vec3  V_tangent = vec3(dot(V, tangent), dot(V, bitangent), dot(V, norm));

		// POM Loop
		float numLayers = mix(24.0, 8.0, clamp(dist / 120.0, 0.0, 1.0));
		float layerDepth = 1.0 / numLayers;
		float currentLayerDepth = 0.0;
		vec2  P = (V_tangent.xy / max(abs(V_tangent.z), 0.05)) * detailHeightScale;
		vec2  deltaTexCoords = P / numLayers;

		vec2  currentTexCoords = FragPos.xz;
		float currentDepthMapValue = sampleProceduralHeightMap(currentTexCoords, bWeights1, bWeights2, 0.0);

		while (currentLayerDepth < currentDepthMapValue && currentLayerDepth < 1.0) {
			currentTexCoords -= deltaTexCoords;
			// Wind sway: apply to grass biomes specifically, using requested division by 5 (0.2)
			vec2 windOff = -rawWindNudge.xz * (1.0 - currentLayerDepth) * 0.2 * grassFactor;
			currentDepthMapValue = sampleProceduralHeightMap(currentTexCoords + windOff, bWeights1, bWeights2, 0.0);
			currentLayerDepth += layerDepth;
		}

		// Refinement
		vec2  prevTexCoords = currentTexCoords + deltaTexCoords;
		float afterDepth = currentDepthMapValue - currentLayerDepth;
		float beforeDepth = sampleProceduralHeightMap(prevTexCoords - rawWindNudge.xz * (1.0 - (currentLayerDepth - layerDepth)) * 0.2 * grassFactor, bWeights1, bWeights2, 0.0) -
			(currentLayerDepth - layerDepth);

		float weight = afterDepth / min(-0.001, (afterDepth - beforeDepth));
		currentTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);
		float finalDepth = clamp(currentLayerDepth - layerDepth + (1.0 - weight) * layerDepth, 0.0, 1.0);
		float finalDetailHeight = 1.0 - finalDepth;

		// Self-shadowing
		vec3  L = normalize(lights[0].position - FragPos);
		vec3  L_tangent = vec3(dot(L, tangent), dot(L, bitangent), dot(L, norm));
		float shadowSteps = 4.0;
		float shadowLayerDepth = finalDepth / shadowSteps;
		vec2  L_delta = (L_tangent.xy / max(abs(L_tangent.z), 0.05)) * detailHeightScale / shadowSteps;
		float currentShadowDepth = finalDepth - shadowLayerDepth;
		vec2  shadowTexCoords = currentTexCoords + L_delta;

		for (int i = 0; i < 4; i++) {
			float h = sampleProceduralHeightMap(shadowTexCoords - rawWindNudge.xz * (1.0 - currentShadowDepth) * 0.2 * grassFactor, bWeights1, bWeights2, 0.0);
			if (h < currentShadowDepth) {
				pomShadow *= mix(1.0, 0.7, grassFactor + rockFactor * 0.5); // Less aggressive shadowing for sand/snow
			}
			currentShadowDepth -= shadowLayerDepth;
			shadowTexCoords += L_delta;
		}

		// Adjust material
		albedo = mix(albedo * (1.0 - 0.7 * (grassFactor + rockFactor * 0.3)), albedo * 1.2, finalDetailHeight);
		roughness = mix(roughness * 1.1, roughness * 0.9, finalDetailHeight);
	}

	// ========================================================================
	// Normal Perturbation (Grain)
	// ========================================================================
	vec3 perturbedNorm = norm;

	if (perturbFactor >= 0.1 && normalStrength > 0.0) {
		float roughnessStrength = smoothstep(0.1, 1.0, perturbFactor) * normalStrength;
		float roughnessScale = normalScale * 0.05;
		vec3  scaledFragPos = FragPos / worldScale;

		// Use finite difference to approximate the gradient of the noise field
		float eps = 0.015;
		float n = fastWorley3d(0.1 * scaledFragPos * roughnessScale);
		float nx = fastWorley3d(0.1 * (scaledFragPos + vec3(eps, 0.0, 0.0)) * roughnessScale);
		float nz = fastWorley3d(0.1 * (scaledFragPos + vec3(0.0, 0.0, eps)) * roughnessScale);

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
	albedo *= mix(1, mix(1.0, 1.25, windDistortion) * mix(1.0, 1.05, windRipple), grassFactor);
	roughness *= mix(1.25, 1.0, windDistortion) * mix(1, mix(1.5, 1.0, windRipple), grassFactor);
	// perturbedNorm += rawWindNudge * mix(0.0, 1.05, plainRipple);

	vec3 lighting = apply_lighting_pbr(FragPos, perturbedNorm, albedo, roughness, metallic, 1.0).rgb;
	lighting *= pomShadow;

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
