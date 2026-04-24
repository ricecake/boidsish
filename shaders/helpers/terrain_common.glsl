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

#ifndef TERRAIN_HEIGHT_MAP_DEFINED
	#define TERRAIN_HEIGHT_MAP_DEFINED
uniform sampler2DArray u_heightmapArray;
#endif

#ifndef TERRAIN_BIOME_MAP_DEFINED
	#define TERRAIN_BIOME_MAP_DEFINED
uniform sampler2DArray u_biomeMap;
#endif

#ifndef TERRAIN_MAX_HEIGHT_GRID_DEFINED
	#define TERRAIN_MAX_HEIGHT_GRID_DEFINED
uniform sampler2D u_maxHeightGrid;
#endif

struct TerrainInfo {
	float height;
	vec3  normal;
	float waterMask;
	int   lowIdx;
	int   highIdx;
	float biomeLerp;
	bool  isBaked;
	bool  isValid;
};

/**
 * Unified function to extract terrain data from a specific texture slice and remapped UV.
 */
TerrainInfo getTerrainInfoFromSlice(float slice, vec2 remappedUV) {
	TerrainInfo info;
	vec4 data = textureLod(u_heightmapArray, vec3(remappedUV, slice), 0.0);
	vec4 biome = textureLod(u_biomeMap, vec3(remappedUV, slice), 0.0);

	info.isBaked = biome.b > 0.5;
	info.height = data.r;

	if (info.isBaked) {
		// Reconstruct normal from XZ components and extract water mask from Alpha
		info.normal = normalize(vec3(data.g, sqrt(max(0.0, 1.0 - data.g * data.g - data.b * data.b)), data.b));
		info.waterMask = data.a;
	} else {
		// Traditional layout: GBA contains normal
		info.normal = normalize(data.gba);
		info.waterMask = info.height < 0.0 ? 1.0 : 0.0;
	}

	info.lowIdx = int(biome.r * 255.0 + 0.5);
	info.highIdx = min(info.lowIdx + 1, 7);
	info.biomeLerp = biome.g;
	info.isValid = true;

	return info;
}

/**
 * Unified function to fetch terrain data for any world position.
 */
TerrainInfo getTerrainInfo(vec2 worldXZ) {
	TerrainInfo info;
	info.height = -10000.0;
	info.normal = vec3(0, 1, 0);
	info.waterMask = 0.0;
	info.isBaked = false;
	info.isValid = false;
	info.lowIdx = 0;
	info.highIdx = 0;
	info.biomeLerp = 0.0;

	if (u_originSize.w < 1)
		return info;

	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;
	vec2  gridPos = worldXZ / scaledChunkSize;
	ivec2 chunkCoord = ivec2(floor(gridPos));
	ivec2 localGridCoord = chunkCoord - u_originSize.xy;

	if (localGridCoord.x < 0 || localGridCoord.x >= u_originSize.z || localGridCoord.y < 0 ||
	    localGridCoord.y >= u_originSize.z) {
		return info;
	}

	int slice = texelFetch(u_chunkGrid, localGridCoord, 0).r;
	if (slice < 0)
		return info;

	vec2 uv = (worldXZ - vec2(chunkCoord) * scaledChunkSize) / scaledChunkSize;
	vec2 remappedUV = (uv * u_terrainParams.x + 0.5) / (u_terrainParams.x + 1.0);

	return getTerrainInfoFromSlice(float(slice), remappedUV);
}

/**
 * Centralized sampling for specific fields, optimized by getTerrainInfo.
 */
float getTerrainHeight(vec2 worldXZ) {
	return getTerrainInfo(worldXZ).height;
}

vec3 getTerrainNormal(vec2 worldXZ) {
	return getTerrainInfo(worldXZ).normal;
}

#endif // HELPERS_TERRAIN_COMMON_GLSL
