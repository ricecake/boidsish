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
layout(binding = [[TERRAIN_CHUNK_GRID_BINDING]]) uniform isampler2D u_chunkGrid;
#endif

#ifndef TERRAIN_MAX_HEIGHT_BINDING_DEFINED
#define TERRAIN_MAX_HEIGHT_BINDING_DEFINED
layout(binding = [[TERRAIN_MAX_HEIGHT_BINDING]]) uniform sampler2D  u_maxHeightGrid;
#endif

#ifndef BAKED_HEIGHTMAP_BINDING_DEFINED
#define BAKED_HEIGHTMAP_BINDING_DEFINED
layout(binding = [[BAKED_HEIGHTMAP_BINDING]]) uniform sampler2DArray u_heightmapArray;
#endif

#ifndef BAKED_PARAMS_BINDING_DEFINED
#define BAKED_PARAMS_BINDING_DEFINED
layout(binding = [[BAKED_PARAMS_BINDING]]) uniform sampler2DArray u_bakedParamsArray;
#endif

#ifndef TERRAIN_BIOME_MAP_DEFINED
#define TERRAIN_BIOME_MAP_DEFINED
layout(binding = [[TERRAIN_BIOME_MAP_BINDING]]) uniform sampler2DArray u_biomeMap;
#endif

// Octahedron normal decoding
vec3 unpackNormal(vec2 p) {
    p = p * 2.0 - 1.0;
    vec3 n = vec3(p.x, 1.0 - abs(p.x) - abs(p.y), p.y);
    float t = max(-n.y, 0.0);
    n.x += n.x >= 0.0 ? -t : t;
    n.z += n.z >= 0.0 ? -t : t;
    return normalize(n);
}

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
	return textureLod(u_heightmapArray, vec3(remappedUV, float(slice)), 0.0).r;
}

/**
 * Get the terrain world position (including 3D displacement) at a specific world position.
 */
vec3 getTerrainWorldPos(vec2 worldXZ) {
	if (u_originSize.w < 1)
		return vec3(worldXZ.x, -10000.0, worldXZ.y);
	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;
	vec2  gridPos = worldXZ / scaledChunkSize;
	ivec2 chunkCoord = ivec2(floor(gridPos));
	ivec2 localGridCoord = chunkCoord - u_originSize.xy;

	if (localGridCoord.x < 0 || localGridCoord.x >= u_originSize.z || localGridCoord.y < 0 ||
	    localGridCoord.y >= u_originSize.z) {
		return vec3(worldXZ.x, -9999.0, worldXZ.y);
	}

	int slice = texelFetch(u_chunkGrid, localGridCoord, 0).r;
	if (slice < 0)
		return vec3(worldXZ.x, -10000.0, worldXZ.y);

	vec2 uv = (worldXZ - vec2(chunkCoord) * scaledChunkSize) / scaledChunkSize;
	vec2 remappedUV = (uv * u_terrainParams.x + 0.5) / (u_terrainParams.x + 1.0);
	vec4 hData = textureLod(u_heightmapArray, vec3(remappedUV, float(slice)), 0.0);

	vec3 pos;
	pos.x = vec2(chunkCoord).x * scaledChunkSize + uv.x * scaledChunkSize + hData.g;
	pos.y = hData.r;
	pos.z = vec2(chunkCoord).y * scaledChunkSize + uv.y * scaledChunkSize + hData.b;
	return pos;
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

	float nx = textureLod(u_heightmapArray, vec3(remappedUV, float(slice)), 0.0).a;
	float ny = textureLod(u_bakedParamsArray, vec3(remappedUV, float(slice)), 0.0).a;

	return unpackNormal(vec2(nx, ny));
}

/**
 * Get the water mask at a specific world position.
 */
float getTerrainWaterMask(vec2 worldXZ) {
	if (u_originSize.w < 1)
		return 0.0;
	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;
	vec2  gridPos = worldXZ / scaledChunkSize;
	ivec2 chunkCoord = ivec2(floor(gridPos));
	ivec2 localGridCoord = chunkCoord - u_originSize.xy;

	if (localGridCoord.x < 0 || localGridCoord.x >= u_originSize.z || localGridCoord.y < 0 ||
	    localGridCoord.y >= u_originSize.z) {
		return 0.0;
	}

	int slice = texelFetch(u_chunkGrid, localGridCoord, 0).r;
	if (slice < 0)
		return 0.0;

	vec2 uv = (worldXZ - vec2(chunkCoord) * scaledChunkSize) / scaledChunkSize;
	vec2 remappedUV = (uv * u_terrainParams.x + 0.5) / (u_terrainParams.x + 1.0);

	return textureLod(u_biomeMap, vec3(remappedUV, float(slice)), 0.0).a;
}

#endif // HELPERS_TERRAIN_COMMON_GLSL
