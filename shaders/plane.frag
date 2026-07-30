#version 460 core
layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 Velocity;
layout(location = 2) out vec4 NormalOut;
layout(location = 3) out vec4 AlbedoOut;

in vec3 WorldPos;
in vec3 Normal;
in vec4 CurPosition;
in vec4 PrevPosition;

#define USE_TERRAIN_DATA
#include "helpers/terrain_shadows.glsl"
#include "helpers/lighting.glsl"
#include "temporal_data.glsl"
#include "helpers/fast_noise.glsl"

void main() {
	// --- Spherical Horizon ---
	float dist = length(WorldPos.xz - viewPos.xz);
	float n_fade = fastSimplex3d(vec3(WorldPos.xz / (250.0 * worldScale), time * 0.09));
	float fade_start = 560.0 * worldScale;
	float fade_end = 570.0 * worldScale;
	float fade = 1.0 - smoothstep(fade_start, fade_end, dist + n_fade * 40.0);

	// Sphere parameters
	float R_p = 50000.0 * worldScale;
	float d_clamped = min(dist, R_p - 1.0);
	float y_sphere = -R_p + sqrt(R_p * R_p - d_clamped * d_clamped);

	// Form the sphere surface position and normal
	vec3 spherePos = vec3(WorldPos.x, y_sphere, WorldPos.z);
	vec3 sphereNormal = normalize(vec3(WorldPos.x - viewPos.x, sqrt(R_p * R_p - d_clamped * d_clamped), WorldPos.z - viewPos.z));

	// Calculate correct fragment depth for the sphere
	vec4 clipPos = projection * view * vec4(spherePos, 1.0);
	gl_FragDepth = clipPos.z / clipPos.w;

	// Mask out the area where the terrain is, and fade out at horizon
	float sphere_alpha = 1.0 - fade;
	float far_fade = 1.0 - smoothstep(zFar * 0.9, zFar, dist);
	sphere_alpha *= far_fade;

	if (sphere_alpha < 0.01) {
		discard;
	}

	// --- Grid logic ---
	float grid_spacing = 1.0;
	vec2  coord = spherePos.xz / grid_spacing;
	vec2  f = fwidth(coord);

	vec2  grid_minor = abs(fract(coord - 0.5) - 0.5) / f;
	float line_minor = min(grid_minor.x, grid_minor.y);
	float C_minor = 1.0 - min(line_minor, 1.0);

	// Thicker/major grid
	vec2  grid_major = abs(fract(coord / 5.0 - 0.5) - 0.5) / f;
	float line_major = min(grid_major.x, grid_major.y);
	float C_major = 1.0 - min(line_major, 1.0);

	float intensity = max(C_minor, C_major * 1.5) * 0.6;
	vec3  grid_color = vec3(0.0, 0.8, 0.8) * intensity;

	// --- Plane lighting ---
	vec3 norm = sphereNormal;
	vec3 surfaceColor = vec3(0.05, 0.05, 0.08);
	float primaryShadow;
	vec3 lighting = apply_lighting(spherePos, norm, surfaceColor, 0.8, primaryShadow).rgb;

	// --- Combine colors ---
	vec3 final_color = lighting * surfaceColor + grid_color;

	// ========================================================================
	// Neon 80s Synth Style (Night Theme) - Matched to terrain.frag
	// ========================================================================
	float gridScale = 0.05; // Lines every 20 units
	vec2  gridUV = spherePos.xz * gridScale;

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

	// Blend final_color towards dark purple/magenta for that 80s look
	vec3 newLighting = mix(final_color, final_color * vec3(0.4, 0.1, 0.5), 0.7);

	// Add cyan grid with magenta glow
	newLighting += gridLine * cyan * 0.8;
	newLighting += gridGlowFactor * magenta * 0.4;
	vec3 gridLight = newLighting;

	// Height-based neon pulse/glow
	float heightGlow = smoothstep(0.0, 100.0 * worldScale, spherePos.y);
	newLighting += magenta * heightGlow * (0.8 + 0.2 * sin(time * 0.5));

	// Mix to Synthwave style
	float nightNoise = fastWorley3d(vec3(spherePos.xy / (25.0 * worldScale), time * 0.08));
	float nightFade = smoothstep(fade_start - 10, fade_end, dist + nightNoise * 100.0);
	vec3 final_lighting = mix(mix(final_color, gridLight, smoothstep(fade_start - 150, fade_end - 20, dist)), newLighting, nightFade);

	// --- Distance Fade / Styling matching terrain ---
	vec4 outColor = vec4(final_lighting, sphere_alpha);
	// Past fade, terrain uses cyan style. Sphere is always past fade start so we apply the cyan style.
	FragColor = vec4(0.0, 0.7, 0.7, sphere_alpha) * length(outColor);

	// Calculate screen-space velocity and material properties using spherePos
	vec4 curPosSphere = projection * view * vec4(spherePos, 1.0);
	vec4 prevPosSphere = prevViewProjection * vec4(spherePos, 1.0);
	vec2 a = (curPosSphere.xy / curPosSphere.w) * 0.5 + 0.5;
	vec2 b = (prevPosSphere.xy / prevPosSphere.w) * 0.5 + 0.5;
	Velocity = vec4(a - b, 0.05, 0.9); // Roughness, Metallic

	// Output view-space normal
	NormalOut = vec4(normalize(mat3(view) * norm), primaryShadow);
	AlbedoOut = vec4(surfaceColor, 1.0);
}
