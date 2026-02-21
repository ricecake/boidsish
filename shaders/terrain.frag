#version 430 core
layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec2 Velocity;

in vec3        Normal;
in vec3        FragPos;
in vec4        CurPosition;
in vec4        PrevPosition;
in vec2        TexCoords;
flat in float TextureSlice;
in float      perturbFactor;
in float      tessFactor;

#include "helpers/lighting.glsl"
#include "helpers/terrain_noise.glsl"

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

#define HEIGHT_PEAK (100.0 * worldScale)
#define HEIGHT_BEACH_END (3.0 * worldScale)

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


void main() {
	if (uIsShadowPass) {
		// Output only depth (handled by hardware)
		return;
	}

	// Scale world-space position for detail noise to match terrain scaling
	vec3 scaledFragPos = FragPos / worldScale;

	// ========================================================================
	// Noise Generation
	// ========================================================================
	float[6] noise = fbm_detail(scaledFragPos * 0.2);

	// Large scale domain warping for natural variation
	// vec3  warp = vec3(fbm(FragPos * 0.01 + FragPos.x * 0.02));
	vec3  warp = vec3(noise[3]);
	float largeNoise = fbm(scaledFragPos * 0.015 + warp * 0.3);

	// Medium scale noise for biome boundaries
	float medNoise = noise[4]; // fbm_detail(FragPos * 0.05);

	// Fine detail noise for texture
	// float fineNoise = fbm_detail(FragPos * 0.2);
	float fineNoise = noise[5];

	// Combined noise for various effects
	float combinedNoise = largeNoise * 0.6 + medNoise * 0.3 + fineNoise * 0.1;

	// float n1 = snoise(vec3(FragPos.xy / 5, FragPos.z * 0.25));
	float n2 = snoise(vec3(FragPos.xy / 25, time * 0.08));
	float n3 = snoise(vec3(FragPos.xy / 50, FragPos.x * 0.04));
	// ========================================================================
	// Distance Fade -- precalc
	// ========================================================================
	vec3  norm = normalize(Normal);
	float dist = length(FragPos.xz - viewPos.xz);
	float fade_start = 560.0 * worldScale;
	float fade_end = 570.0 * worldScale;
	float fade = 1.0 - smoothstep(fade_start, fade_end, dist + n2 * 40.0);

	if (fade < 0.2) {
		discard;
	}

	/*
/*
	    // ========================================================================
	    // Terrain Analysis
	    // ========================================================================
*/

	// float largeNoise = n1;
	// float medNoise =  n2;
	// float fineNoise = n3;
	// float combinedNoise = largeNoise * 0.6 + medNoise * 0.3 + fineNoise * 0.1;
	// Height with noise distortion for natural boundaries
	float baseHeight = FragPos.y;
	float distortedHeight = baseHeight + largeNoise * 5.0 * worldScale;

	// Slope analysis: 1.0 = flat horizontal, 0.0 = vertical cliff
	float slope = dot(norm, vec3(0.0, 1.0, 0.0));
	float distortedSlope = slope + medNoise * 0.08;

	// Valley/ridge detection
	float valleyFactor = calculateValleyFactor(FragPos);

	// Moisture calculation
	float moisture = calculateMoisture(baseHeight, valleyFactor, FragPos);

	// Valley lushness boost
	float valleyLushness = clamp(-valleyFactor, 0.0, 1.0);
	moisture = mix(moisture, min(moisture + 0.3, 1.0), valleyLushness);

	// ========================================================================
	// Material Calculation (Unified Biome Map)
	// ========================================================================

	// Get biome data from texture
	vec2  biomeData = texture(uBiomeMap, vec3(TexCoords, TextureSlice)).rg;
	int   lowIdx = int(biomeData.r * 255.0 + 0.5);
	int   highIdx = min(lowIdx + 1, 7);
	float t = biomeData.g;

	vec3  albedo = mix(biomes[lowIdx].albedo_roughness.rgb, biomes[highIdx].albedo_roughness.rgb, t);
	float roughness = mix(biomes[lowIdx].albedo_roughness.a, biomes[highIdx].albedo_roughness.a, t);
	float metallic = mix(biomes[lowIdx].params.x, biomes[highIdx].params.x, t);

	// Slope-based cliff fallback (optional, but keep it for character)
	float verticalMask = smoothstep(0.4, 0.2, slope);
	if (verticalMask > 0.1) {
		vec3 cliffAlbedo = mix(vec3(0.35, 0.3, 0.25), vec3(0.45, 0.45, 0.48), n3);
		albedo = mix(albedo, cliffAlbedo, verticalMask);
		roughness = mix(roughness, 0.6, verticalMask);
	}

	TerrainMaterial finalMaterial;
	finalMaterial.albedo = albedo;
	finalMaterial.roughness = roughness;
	finalMaterial.metallic = metallic;
	finalMaterial.normalScale = mix(biomes[lowIdx].params.z, biomes[highIdx].params.z, t);
	finalMaterial.normalStrength = mix(biomes[lowIdx].params.y, biomes[highIdx].params.y, t);

	vec3 finalAlbedo = finalMaterial.albedo;
	// ========================================================================
	// Detail Variation
	// ========================================================================

	// Add subtle color variation based on fine noise
	float colorVar = fineNoise * 0.1;
	finalAlbedo *= (1.0 + colorVar);

	// Darken valleys slightly (shadow accumulation)
	float valleyShadow = clamp(-valleyFactor * 0.15, 0.0, 0.1);
	finalAlbedo *= (1.0 - valleyShadow);

	// ========================================================================
	// Lighting
	// ========================================================================
	// */

	// vec3  warp = vec3(fbm(FragPos / 50 + time * 0.08));
	// float nebula_noise = fbm(FragPos / 50 + warp * 0.8);
	// float funky_noise = fbm(FragPos / 20 + warp.zxy * 1.8);

	// vec3 finalAlbedo = getBiomeColor(FragPos.y + n1, n1+n3, n3);

	//    vec3 bonusColor = mix(vec3(0.1, 0.4, 0.2), vec3(0.1, 0.5, 0.2), FragPos.y / 100);
	//    finalAlbedo = mix(finalAlbedo, bonusColor, nebula_noise);
	// finalAlbedo = mix(vec3(0.2), finalAlbedo, 5*dot(norm, vec3(0,1,0)));
	//    finalAlbedo = mix(finalAlbedo, vec3(1,0.5,0.25), fwidth(norm));
	// finalAlbedo = mix(finalAlbedo, vec3(1,0.5,0.25), fwidth(norm)); // need something that can be a biome specific
	// "flower" Curvature highlighting - clamped and dampened to avoid extreme artifacts in depressions Curvature
	// highlighting - clamped and significantly dampened to avoid extreme artifacts in depressions fwidth(norm) can be
	// noisy on steep tessellated slopes, contributing to "leopard print" artifacts.
	//    float curvature = clamp(length(fwidth(norm)), 0.0, 1.0);
	//    finalAlbedo = mix(finalAlbedo, vec3(1,0.5,0.25), curvature * 0.05);

	// ========================================================================
	// Normal Perturbation (Grain)
	// ========================================================================
	// Enhance realism by adding fine-grained surface roughness.
	// Scale and strength are now tied to the material/biome.
	vec3 perturbedNorm = norm;
	if (perturbFactor >= 0.1) {
		float roughnessStrength = smoothstep(0.1, 1.0, perturbFactor) * finalMaterial.normalStrength;
		float roughnessScale = finalMaterial.normalScale * 0.05;

		// Use finite difference to approximate the gradient of the noise field
		float eps = 0.015;
		float n = snoise(scaledFragPos * roughnessScale);
		float nx = snoise((scaledFragPos + vec3(eps, 0.0, 0.0)) * roughnessScale);
		float nz = snoise((scaledFragPos + vec3(0.0, 0.0, eps)) * roughnessScale);

		// Compute local tangent space to orient the perturbation
		vec3 tangent = normalize(cross(norm, vec3(0, 0, 1)));
		if (abs(norm.z) > 0.9)
			tangent = normalize(cross(norm, vec3(1, 0, 0)));
		vec3 bitangent = cross(norm, tangent);

		// Apply perturbation based on noise gradient
		perturbedNorm = normalize(norm + (tangent * (n - nx) + bitangent * (n - nz)) * (roughnessStrength / eps));
	}
	// vec3 lighting = apply_lighting(FragPos, norm, finalAlbedo, 0.8);
	vec3 lighting =
		apply_lighting_pbr(FragPos, perturbedNorm, finalAlbedo, finalMaterial.roughness, finalMaterial.metallic, 1.0)
			.rgb;

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
		lighting = mix(lighting, lighting * vec3(0.4, 0.1, 0.5), nightFactor * 0.7);

		// Add cyan grid with magenta glow
		lighting += gridLine * cyan * nightFactor * 0.8;
		lighting += gridGlowFactor * magenta * nightFactor * 0.4;

		// Height-based neon pulse/glow
		float heightGlow = smoothstep(0.0, 100.0 * worldScale, FragPos.y);
		lighting += magenta * heightGlow * nightFactor * (0.8 + 0.2 * sin(time * 0.5));
	}

	// ========================================================================
	// Distance Fade
	// ========================================================================
	vec4 outColor = vec4(lighting, mix(0.0, fade, step(0.01, FragPos.y))) + vec4(smoothstep(0.2, 0.5, 1 - fade));
	FragColor = mix(
		vec4(0.0, 0.7, 0.7, mix(0.0, fade, step(0.01, FragPos.y))) * length(outColor),
		outColor,
		step(1.0, fade)
	);

	// Calculate screen-space velocity
	vec2 a = (CurPosition.xy / CurPosition.w) * 0.5 + 0.5;
	vec2 b = (PrevPosition.xy / PrevPosition.w) * 0.5 + 0.5;
	Velocity = a - b;

	// float heat = clamp(tessFactor / 32.0, 0.0, 1.0);
	// FragColor = vec4(heat, 1.0 - heat, 0.0, 1.0); // Simple Red-Green ramp

	// FragColor = vec4(tessFactor, norm.y, dist, 1.0);
	// FragColor = vec4(smoothstep(0,16, tessFactor), smoothstep(8,24, tessFactor), smoothstep(16,32, tessFactor), 1.0);
	// FragColor = vec4(tessFactor, 0,0, 1.0);

	// vec4 outColor = vec4(lighting, fade);
	// FragColor = mix(
	//      vec4(0.0, 0.7, 0.7, fade) * length(outColor),
	//      outColor,
	//      step(1.0, fade)
	// );
}
