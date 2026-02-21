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

void main() {
	if (uIsShadowPass) {
		// Output only depth (handled by hardware)
		return;
	}

	// Distance Fade -- precalc
	vec3  norm = normalize(Normal);
	float dist = length(FragPos.xz - viewPos.xz);
	float n_fade = snoise(vec3(FragPos.xy / 25, time * 0.08));
	float fade_start = 560.0 * worldScale;
	float fade_end = 570.0 * worldScale;
	float fade = 1.0 - smoothstep(fade_start, fade_end, dist + n_fade * 40.0);

	if (fade < 0.2) {
		discard;
	}

	// Slope analysis: 1.0 = flat horizontal, 0.0 = vertical cliff
	float slope = dot(norm, vec3(0.0, 1.0, 0.0));

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

	// Slope-based cliff blending
	float verticalMask = smoothstep(0.4, 0.2, slope);
	if (verticalMask > 0.1) {
		vec3 cliffAlbedo = mix(vec3(0.35, 0.3, 0.25), vec3(0.45, 0.45, 0.48), 0.5);
		albedo = mix(albedo, cliffAlbedo, verticalMask);
		roughness = mix(roughness, 0.6, verticalMask);
	}

	// Final Lighting
	vec3 lighting = apply_lighting_pbr(FragPos, norm, albedo, roughness, metallic, 1.0).rgb;

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
	vec4 outColor = vec4(lighting, mix(0.0, fade, step(0.01, FragPos.y))) + vec4(smoothstep(0.2, 0.5, 1.0 - fade));
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
