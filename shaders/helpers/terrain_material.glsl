#ifndef TERRAIN_MATERIAL_GLSL
#define TERRAIN_MATERIAL_GLSL

#include "terrain_common_structs.glsl"

#ifndef TERRAIN_MATERIAL_STRUCTS
#define TERRAIN_MATERIAL_STRUCTS
struct BiomeProperties {
	vec4 albedo_roughness; // rgb = albedo, w = roughness
	vec4 params;           // x = metallic, y = detailStrength, z = detailScale, w = unused
};

layout(std140, binding = 7) uniform BiomeData {
	BiomeProperties biomes[8];
};
#endif

// Constants from terrain.frag
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
const vec3 COL_DIRT = vec3(0.35, 0.25, 0.18);

struct TerrainMaterial {
	vec3  albedo;
	float roughness;
	float metallic;
	float normalScale;
	float normalStrength;
};

/**
 * Calculate valley/ridge factor using noise-based curvature approximation.
 */
float calculateValleyFactor(vec3 pos) {
	// Use texture-based noise for performance
	float scale = 0.02;
	float center = fastSimplex3d(pos * scale);

	// Sample neighbors for Laplacian approximation
	float dx = 5.0;
	float north = fastSimplex3d((pos + vec3(0, 0, dx)) * scale);
	float south = fastSimplex3d((pos - vec3(0, 0, dx)) * scale);
	float east = fastSimplex3d((pos + vec3(dx, 0, 0)) * scale);
	float west = fastSimplex3d((pos - vec3(dx, 0, 0)) * scale);

	float laplacian = (north + south + east + west) / 4.0 - center;
	return laplacian * 10.0;
}

/**
 * Calculate moisture based on height, valley factor, and noise
 */
float calculateMoisture(float height, float valleyFactor, vec3 pos) {
	// Base moisture decreases with altitude (less rain at high elevations)
	float baseMoisture = 1.0 - smoothstep(0.0, HEIGHT_PEAK, height) * 0.6;

	// Valleys are more moist (water collects there)
	float valleyMoisture = clamp(-valleyFactor * 0.5, 0.0, 0.4);

	// Add some noise variation (texture-based)
	float noiseMoisture = fastSimplex3d(pos * 0.03) * 0.2;

	return clamp(baseMoisture + valleyMoisture + noiseMoisture, 0.0, 1.0);
}

/**
 * Get the base biome material based on height
 */
TerrainMaterial getBiomeMaterial(float height, float moisture, float noise) {
	TerrainMaterial mat;
	mat.metallic = 0.0;
	float h = height + noise * 8.0;

	if (h < HEIGHT_BEACH_END) {
		float wetness = 1.0 - smoothstep(0.0, HEIGHT_BEACH_END, h);
		mat.albedo = mix(COL_SAND_DRY, COL_SAND_WET, wetness);
		mat.roughness = mix(0.9, 0.4, wetness);
		mat.normalScale = 40.0;
		mat.normalStrength = mix(0.1, 0.05, wetness);
		return mat;
	}

	if (h < HEIGHT_LOWLAND_END) {
		float t = smoothstep(HEIGHT_BEACH_END, HEIGHT_LOWLAND_END, h);
		vec3  grassColor = mix(COL_GRASS_LUSH, COL_GRASS_DRY, t * (1.0 - moisture));
		float grassRoughness = mix(0.7, 0.8, t * (1.0 - moisture));
		float sandFade = smoothstep(HEIGHT_BEACH_END, HEIGHT_BEACH_END + 5.0, h);
		mat.albedo = mix(COL_SAND_DRY, grassColor, sandFade);
		mat.roughness = mix(0.9, grassRoughness, sandFade);
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

/**
 * Calculate cliff/steep surface material properties
 */
TerrainMaterial getCliffMaterial(float height, float noise) {
	TerrainMaterial mat;
	mat.metallic = 0.0;
	float h = height + noise * 5.0;

	if (h < HEIGHT_FOREST_END) {
		float wetness = 0.3 + noise * 0.2;
		mat.albedo = mix(COL_ROCK_BROWN, COL_ROCK_DARK, wetness);
		mat.roughness = mix(0.6, 0.3, wetness);
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

#endif // TERRAIN_MATERIAL_GLSL
