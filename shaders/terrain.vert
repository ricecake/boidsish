#version 430 core

// Per-vertex attributes from persistent VBO
layout(location = 0) in vec3 aPos;    // Absolute world position
layout(location = 1) in vec3 aNormal; // Normal
layout(location = 2) in vec2 aBiome;  // Biome data (lowIdx, t)

out vec3       FragPos;
out vec4       CurPosition;
out vec4       PrevPosition;
out vec3       Normal;
out vec2       TexCoords;
flat out float TextureSlice;
out float      perturbFactor;
out float      tessFactor;

uniform mat4 view;
uniform mat4 projection;
uniform vec4 clipPlane;

#include "helpers/shockwave.glsl"
#include "helpers/terrain_noise.glsl"
#include "temporal_data.glsl"
#include "visual_effects.glsl"

void main() {
	FragPos = aPos;
	Normal = normalize(aNormal);

	// Pass biome data to fragment shader via TexCoords
	TexCoords = aBiome;

	// Apply shockwave ripple
	FragPos += getShockwaveDisplacement(FragPos, 0.0, false);

	// Clip plane for reflections/refractions
	gl_ClipDistance[0] = dot(FragPos, clipPlane.xyz) + clipPlane.w;

	// Tessellation factors are no longer relevant, but fragment shader expects them.
	perturbFactor = 1.0;
	tessFactor = 1.0;

	// TextureSlice is used for biome texture lookup in the old system.
	// We pass 0 here; terrain.frag was updated to use interpolated biome data.
	TextureSlice = 0.0;

	gl_Position = projection * view * vec4(FragPos, 1.0);
	CurPosition = gl_Position;

	// PrevPosition for TAA/Motion blur (assuming static terrain)
	PrevPosition = prevViewProjection * vec4(FragPos, 1.0);
}
