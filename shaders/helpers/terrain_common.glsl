#ifndef HELPERS_TERRAIN_COMMON_GLSL
#define HELPERS_TERRAIN_COMMON_GLSL

#ifndef TERRAIN_DATA_BLOCK
#define TERRAIN_DATA_BLOCK
layout(std140, binding = [[TERRAIN_DATA_BINDING]]) uniform TerrainData {
	ivec4 u_originSize;    // x, z, size, is_bound
	vec4  u_terrainParams; // chunkSize, worldScale
};
#endif

#ifndef TERRAIN_GRID_DEFINED
	#define TERRAIN_GRID_DEFINED
uniform isampler2D u_chunkGrid;
#endif

uniform sampler2D  u_maxHeightGrid;
uniform sampler2DArray u_heightmapArray;

/**
 * Get the terrain height at a specific world position.
 */
float getTerrainHeight(vec2 worldXZ) {
	if (u_originSize.w < 1)
		return -10000.0;
	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;
	vec2  gridPos = worldXZ / scaledChunkSize;
	ivec2 chunkCoord = ivec2(floor(gridPos));
	ivec2 localGridCoord = chunkCoord - u_originSize.xy;

	if (localGridCoord.x < 0 || localGridCoord.x >= u_originSize.z || localGridCoord.y < 0 ||
	    localGridCoord.y >= u_originSize.z) {
		return -9999.0;
	}

	int slice = texelFetch(u_chunkGrid, localGridCoord, 0).r;
	if (slice < 0)
		return -10000.0;

	vec2 uv = (worldXZ - vec2(chunkCoord) * scaledChunkSize) / scaledChunkSize;
	vec2 remappedUV = (uv * u_terrainParams.x + 0.5) / (u_terrainParams.x + 1.0);
	return texture(u_heightmapArray, vec3(remappedUV, float(slice))).r;
}

/**
 * Get the terrain normal at a specific world position.
 */
vec3 getTerrainNormal(vec2 worldXZ) {
	if (u_originSize.w < 1)
		return vec3(0.0, 1.0, 0.0);
	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;
	vec2  gridPos = worldXZ / scaledChunkSize;
	ivec2 chunkCoord = ivec2(floor(gridPos));
	ivec2 localGridCoord = chunkCoord - u_originSize.xy;

	if (localGridCoord.x < 0 || localGridCoord.x >= u_originSize.z || localGridCoord.y < 0 ||
	    localGridCoord.y >= u_originSize.z) {
		return vec3(0.0, 1.0, 0.0);
	}

	int slice = texelFetch(u_chunkGrid, localGridCoord, 0).r;
	if (slice < 0)
		return vec3(0.0, 1.0, 0.0);

	vec2 uv = (worldXZ - vec2(chunkCoord) * scaledChunkSize) / scaledChunkSize;
	vec2 remappedUV = (uv * u_terrainParams.x + 0.5) / (u_terrainParams.x + 1.0);
	// Normal is stored in GBA of the heightmap array
	return texture(u_heightmapArray, vec3(remappedUV, float(slice))).gba;
}

#endif // HELPERS_TERRAIN_COMMON_GLSL
