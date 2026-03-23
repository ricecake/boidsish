#version 430 core

struct TerrainVertex {
	vec4 position; // xyz = absolute world position, w = pad
	vec4 normal;   // xyz = normal, w = pad
	vec4 biome;    // xy = biome indices/weights, zw = pad
};

layout(std430, binding = 40) readonly buffer TerrainVertices {
	TerrainVertex vertices[];
};

out vec3 FragPos;
out vec4 CurPosition;
out vec4 PrevPosition;
out vec3 Normal;
out vec2 TexCoords;
out vec2 vBiomeData;
out vec3 viewForward;

uniform mat4 view;
uniform mat4 projection;
uniform mat4 prevViewProjection;
uniform float uChunkSize;

#include "helpers/shockwave.glsl"

void main() {
	// gl_VertexID is used to fetch vertex data from the persistent SSBO
	TerrainVertex v = vertices[gl_VertexID];

	FragPos = v.position.xyz;
	Normal = v.normal.xyz;
	vBiomeData = v.biome.xy;

	// Extract camera forward vector
	viewForward = vec3(-view[0][2], -view[1][2], -view[2][2]);

	// Apply shockwave ripple to terrain
	FragPos += getShockwaveDisplacement(FragPos, 0.0, false);

	// Final vertex position
	gl_Position = projection * view * vec4(FragPos, 1.0);

	CurPosition = gl_Position;
	PrevPosition = prevViewProjection * vec4(FragPos, 1.0);

	// TexCoords can be reconstructed or passed from mesh gen.
	// For now, let's assume world-space based tiling in the fragment shader
	// or use the absolute position for noise.
	TexCoords = FragPos.xz / uChunkSize;
}
