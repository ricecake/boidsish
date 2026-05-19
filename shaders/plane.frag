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
#include "helpers/fader.glsl"
#include "temporal_data.glsl"

uniform mat4 view;

void main() {
	vec3 norm = normalize(Normal);
	float dist = length(WorldPos.xz - viewPos.xz);

	// --- Plane lighting ---
	vec3 surfaceColor = vec3(0.05, 0.05, 0.08);
	float primaryShadow;
	vec3 lighting = apply_lighting(WorldPos, norm, surfaceColor, 0.8, primaryShadow).rgb;

	// --- Combine colors ---
	vec3 final_color = applyWaterGrid(lighting * surfaceColor, WorldPos, norm, dist, time);

	// --- Distance Fade ---
	FragColor = applyStylisticFade(final_color, WorldPos, dist, worldScale, time);

	// Calculate screen-space velocity and material properties
	vec2 a = (CurPosition.xy / CurPosition.w) * 0.5 + 0.5;
	vec2 b = (PrevPosition.xy / PrevPosition.w) * 0.5 + 0.5;
	Velocity = vec4(a - b, 0.05, 0.9); // Roughness, Metallic

	// Output view-space normal
	NormalOut = vec4(normalize(mat3(view) * norm), primaryShadow);
	AlbedoOut = vec4(surfaceColor, 1.0);
}
