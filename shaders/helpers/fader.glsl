#ifndef HELPERS_FADER_GLSL
#define HELPERS_FADER_GLSL

#include "fast_noise.glsl"
#include "../types/terrain.glsl"

struct FaderSettings {
	float fade;
	float fade_start;
	float fade_end;
	float n_fade;
	float dist;
};

FaderSettings newFaderSettings(vec3 FragPos, vec3 viewPos, float time, float worldScale) {
	float dist = length(FragPos.xz - viewPos.xz);

	// Use UBO values if available, otherwise fallback to defaults
	float fade_start = u_terrainParams.z > 0.1 ? u_terrainParams.z : 560.0 * worldScale;
	float fade_end = u_terrainParams.w > 0.1 ? u_terrainParams.w : 570.0 * worldScale;

	float n_fade = fastSimplex3d(vec3(FragPos.xz / (250.0 * worldScale), time * 0.09));
	float fade = 1.0 - smoothstep(fade_start, fade_end, dist + n_fade * 40.0);

	return FaderSettings(fade, fade_start, fade_end, n_fade, dist);
}

bool shouldDiscard(FaderSettings fs) {
	return fs.fade < 0.01;
}

/**
 * Applies the 80s stylistic neon grid and distance fade effects.
 * Includes height-based neon glow and distance-based alpha transition.
 */
vec4 applyStylisticFade(vec4 color, FaderSettings fs, vec3 FragPos, float time, float worldScale, bool enableGrid, bool useHeightAlpha) {
	vec3 lighting = color.rgb;
	float dist = fs.dist;
	float fade_start = fs.fade_start;
	float fade_end = fs.fade_end;
	float fade = fs.fade;

	// ========================================================================
	// Neon 80s Synth Style (Night Theme)
	// ========================================================================
	if (enableGrid) {
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

		float nightNoise = fastWorley3d(vec3(FragPos.xy / (25.0 * worldScale), time * 0.08));
		float nightFade = smoothstep(fade_start - 10.0, fade_end, dist + nightNoise * 100.0);
		lighting = mix(mix(lighting, gridLight, smoothstep(fade_start - 150.0, fade_end - 20.0, dist)), newLighting, nightFade);
	}

	// ========================================================================
	// Distance Fade
	// ========================================================================
	// We handle the alpha fade for transition to the skybox.
	float alpha = fade;
	if (useHeightAlpha) {
		// We keep a minimum alpha for objects above the water line to avoid harsh discards.
		// For water and plane, we don't apply the y-step to keep them visible at y=0.
		alpha = mix(0.0, fade, step(0.01, FragPos.y));
		if (FragPos.y < 0.02 && FragPos.y > -0.02) alpha = fade; // Near y=0 (water/plane)
	}

	vec4 baseColor = vec4(lighting, alpha);

	// Restore deliberate cyan style for distant terrain
	return mix(vec4(0.0, 0.7, 0.7, baseColor.a) * length(baseColor.rgb), baseColor, step(1.0, fade));
}

#endif // HELPERS_FADER_GLSL
