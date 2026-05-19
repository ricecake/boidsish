#ifndef HELPERS_FADER_GLSL
#define HELPERS_FADER_GLSL

#include "fast_noise.glsl"

/**
 * Applies a stylistic neon synthwave fade to the lighting.
 * Handles distance-based alpha fading, grid lines, and height-based glow.
 */
vec4 applyStylisticFade(vec3 lighting, vec3 fragPos, float dist, float worldScale, float time) {
	float fade_start = 560.0 * worldScale;
	float fade_end = 570.0 * worldScale;

	float n_fade = fastSimplex3d(vec3(fragPos.xz / (250 * worldScale), time * 0.09));
	float fade = 1.0 - smoothstep(fade_start, fade_end, dist + n_fade * 40.0);

	// ========================================================================
	// Neon 80s Synth Style (Night Theme)
	// ========================================================================
	// Synthwave grid lines
	float gridScale = 0.05; // Lines every 20 units
	vec2  gridUV = fragPos.xz * gridScale;

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
	float heightGlow = smoothstep(0.0, 100.0 * worldScale, fragPos.y);
	newLighting += magenta * heightGlow * (0.8 + 0.2 * sin(time * 0.5));

	float nightNoise = fastWorley3d(vec3(fragPos.xz / (25 * worldScale), time * 0.08));
	float nightFade = smoothstep(fade_start - 10, fade_end, dist + nightNoise * 100.0);

	vec3 finalLighting = mix(mix(lighting, gridLight, smoothstep(fade_start - 150, fade_end - 20, dist)), newLighting, nightFade);

	// Distance Fade - Alpha Handling
	// step(0.01, fragPos.y) guard allows fading transition to the skybox while preventing
	// objects at or near the water line (y ≈ 0) from being discarded or becoming invisible.
	vec4 baseColor = vec4(finalLighting, mix(0.0, fade, step(0.01, fragPos.y)));

	// Restore deliberate cyan style for distant terrain
	return mix(vec4(0.0, 0.7, 0.7, baseColor.a) * length(baseColor), baseColor, step(1.0, fade));
}

/**
 * Calculates a scrolling grid effect, often used for water or holographic planes.
 */
vec3 applyWaterGrid(vec3 surfaceColor, vec3 fragPos, vec3 norm, float dist, float time) {
	float grid_spacing = 1.0;

	// Awareness of water surface: ripple depth is fragPos.y
	float rippleHeight = fragPos.y;

	// Calculate refraction offset based on surface normal and depth (distance from y=0)
	vec2 refractionOffset = norm.xz * abs(rippleHeight) * 4.0;
	if (dist <= 75.0) {
		refractionOffset = fastCurl3d(vec3(norm.xz / 100.0, rippleHeight)).xz * abs(rippleHeight) * 2.0 *
			smoothstep(75.0, 50.0, dist);
	}
	vec2 coord = (fragPos.xz + refractionOffset) / grid_spacing;
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

	return surfaceColor + grid_color;
}

#endif // HELPERS_FADER_GLSL
