#ifndef TERRAIN_SHADOWS_GLSL
#define TERRAIN_SHADOWS_GLSL

layout(std140, binding = 8) uniform TerrainData {
	ivec4 u_originSize;    // x, y=z, z=size, w=isBound
	vec4  u_terrainParams; // x=chunkSize, y=worldScale
};

uniform isampler2D    u_chunkGrid;
uniform sampler2D     u_maxHeightGrid;
// u_heightmapArray is bound to unit 13
uniform sampler2DArray u_heightmapArray;

float getTerrainHeight(vec2 worldXZ) {
	if (u_originSize.w < 1) return -10000.0;
	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;
	vec2  gridPos = worldXZ / scaledChunkSize;
	ivec2 chunkCoord = ivec2(floor(gridPos));
	ivec2 localGridCoord = chunkCoord - u_originSize.xy;

	if (localGridCoord.x < 0 || localGridCoord.x >= u_originSize.z ||
	    localGridCoord.y < 0 || localGridCoord.y >= u_originSize.z) {
		return -9999.0; // Debug value
	}

	int slice = texelFetch(u_chunkGrid, localGridCoord, 0).r;
	if (slice < 0)
		return -10000.0;

	vec2 uv = (worldXZ - vec2(chunkCoord) * scaledChunkSize) / scaledChunkSize;
	return texture(u_heightmapArray, vec3(uv, float(slice))).r;
}

bool isPointInTerrainShadow(vec3 worldPos, vec3 lightDir) {
	if (u_originSize.w < 1) return false;
	// lightDir is from fragment to light
	if (lightDir.y <= 0.0)
		return false;

	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;
	// Use a small bias to avoid self-shadowing acne
	float t = 0.5 * u_terrainParams.y;
	float maxDist = 800.0 * u_terrainParams.y;

	int iter = 0;
	while (t < maxDist && iter < 64) {
		iter++;
		vec3  p = worldPos + t * lightDir;
		vec2  gridPos = p.xz / scaledChunkSize;
		ivec2 chunkCoord = ivec2(floor(gridPos));
		ivec2 localGridCoord = chunkCoord - u_originSize.xy;

		if (localGridCoord.x < 0 || localGridCoord.x >= u_originSize.z ||
		    localGridCoord.y < 0 || localGridCoord.y >= u_originSize.z) {
			break; // Out of grid bounds
		}

		// Hierarchical Skipping: Sample a coarse mip to see if we're entirely above this 8x8 chunk area
		vec2  gridUV = (vec2(localGridCoord) + 0.5) / float(u_originSize.z);
		float h_max = textureLod(u_maxHeightGrid, gridUV, 3.0).r;

		if (p.y > h_max) {
			// Skip distance is the vertical height difference divided by light slope (sin of altitude)
			// This is conservative because we only consider the max height of the 8x8 region.
			float skip_dist = (p.y - h_max) / lightDir.y;
			t += max(skip_dist, 8.0 * scaledChunkSize);
			continue;
		}

		// At LOD 0, check actual chunk height
		int slice = texelFetch(u_chunkGrid, localGridCoord, 0).r;
		if (slice >= 0) {
			vec2  uv_chunk = (p.xz - vec2(chunkCoord) * scaledChunkSize) / scaledChunkSize;
			float h = texture(u_heightmapArray, vec3(uv_chunk, float(slice))).r;
			if (p.y < h)
				return true; // Hit terrain!
		}

		// Use a smaller step at LOD 0 for accuracy
		t += 1.0 * u_terrainParams.y;
	}

	return false;
}

int isPointInTerrainShadowDebug(vec3 worldPos, vec3 lightDir) {
	if (u_originSize.w < 1) return -3; // Blue
	if (u_terrainParams.y <= 0.0) return -1; // Cyan
	if (u_terrainParams.x <= 0.0) return -4; // White (Invalid chunkSize)
	if (lightDir.y <= 0.0) return -2; // Orange-ish (Light below horizon)

	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;
	float t = 0.5 * u_terrainParams.y;
	float maxDist = 800.0 * u_terrainParams.y;

	int iter = 0;
	while (t < maxDist && iter < 64) {
		iter++;
		vec3  p = worldPos + t * lightDir;
		vec2  gridPos = p.xz / scaledChunkSize;
		ivec2 chunkCoord = ivec2(floor(gridPos));
		ivec2 localGridCoord = chunkCoord - u_originSize.xy;

		if (localGridCoord.x < 0) return 11;
		if (localGridCoord.x >= u_originSize.z) return 12;
		if (localGridCoord.y < 0) return 13;
		if (localGridCoord.y >= u_originSize.z) return 14;

		if (u_originSize.z != 128) return 5; // White/Debug: Invalid grid size

		// Hierarchical check in debug mode
		vec2  gridUV = (vec2(localGridCoord) + 0.5) / float(u_originSize.z);
		float h_max = textureLod(u_maxHeightGrid, gridUV, 3.0).r;
		if (p.y > h_max) {
			float skip_dist = (p.y - h_max) / lightDir.y;
			t += max(skip_dist, 8.0 * scaledChunkSize);
			continue;
		}

		int slice = texelFetch(u_chunkGrid, localGridCoord, 0).r;
		if (slice < 0) {
			return 2; // No slice (Yellow)
		}

		vec2  uv_chunk = (p.xz - vec2(chunkCoord) * scaledChunkSize) / scaledChunkSize;
		float h = texture(u_heightmapArray, vec3(uv_chunk, float(slice))).r;
		if (p.y < h)
			return 3; // Hit! (Magenta)

		t += 1.0 * u_terrainParams.y;
	}

	return 0; // Miss (Green)
}

#endif
