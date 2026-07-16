#version 460 core

#include "helpers/constants.glsl"
#include "helpers/lighting.glsl"
#include "temporal_data.glsl"
#include "helpers/shockwave.glsl"

struct TerrainVertex {
	vec4 position; // xyz = position, w = slice
	vec4 normal;   // xyz = normal, w = unused
	vec4 biome;    // x = low_idx, y = t, z = waterMask, w = isBaked (1.0)
	vec4 params;   // x = erosionDelta, y = ridgeMap, z = substrate, w = deviation
};

layout(std430, binding = [[TERRAIN_PATCH_TESS_LEVELS_BINDING]]) readonly buffer BakedVertices {
	TerrainVertex bakedVertices[];
};

out vec3       Normal;
out vec3       FragPos;
out vec4       CurPosition;
out vec4       PrevPosition;
out vec2       TexCoords;
flat out float TextureSlice;
out float      perturbFactor;
out float      tessFactor;
out float      vIsWater;
out float      vErosionDelta;
out float      vRidgeMap;
out float      vSubstrate;
out float      vIsBaked;

uniform vec4 clipPlane;

#ifndef TERRAIN_DATA_BLOCK
#define TERRAIN_DATA_BLOCK
layout(std140, binding = [[TERRAIN_DATA_BINDING]]) uniform TerrainData {
	ivec4 u_originSize;    // x, z, size, is_bound
	vec4  u_terrainParams; // chunkSize, worldScale
};
#endif

void main() {
	int vertexIdx = gl_VertexID;
	TerrainVertex v = bakedVertices[vertexIdx];

	FragPos = v.position.xyz;
	Normal = v.normal.xyz;
	TextureSlice = v.position.w;

	vIsBaked = v.biome.w;
	vIsWater = v.biome.z;
	vErosionDelta = v.params.x;
	vRidgeMap = v.params.y;
	vSubstrate = v.params.z;

	float worldScale = u_terrainParams.y;
	float chunkSize = u_terrainParams.x * worldScale;
	vec2 localPos = mod(v.position.xz, chunkSize);
	TexCoords = localPos / chunkSize;

	perturbFactor = 1.0;
	tessFactor = 64.0;

	vec3 displacedFragPos = FragPos + getShockwaveDisplacement(FragPos, 0.0, false);
	FragPos = displacedFragPos;

	gl_Position = projection * view * vec4(FragPos, 1.0);
	CurPosition = gl_Position;
	PrevPosition = prevViewProjection * vec4(FragPos, 1.0);

	// Clip plane for reflections
	gl_ClipDistance[0] = dot(FragPos, clipPlane.xyz) + clipPlane.w;
}
