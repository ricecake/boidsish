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

float terrainShadowCoverage(vec3 worldPos, vec3 normal, vec3 lightDir) {
	if (u_originSize.w < 1) return 1.0;
	// lightDir is from fragment to light
	// if (sundownShadow < 1.00) {
	if (lightDir.y <= 0.0) {
		return 0.0;
	}

	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;

	// Better initial bias: move along normal and a bit along light direction.
	// This dramatically reduces shadow acne.
	vec3 p_start = worldPos + normal * (0.2 * u_terrainParams.y) + lightDir * (0.1 * u_terrainParams.y);
	float t = 0.0;
	float maxDist = 1200.0 * u_terrainParams.y;

	float closest = 1.0;
	int iter = 0;
	while (t < maxDist && iter < 80) {
		iter++;
		vec3  p = p_start + t * lightDir;
		vec2  gridPos = p.xz / scaledChunkSize;
		ivec2 chunkCoord = ivec2(floor(gridPos));
		ivec2 localGridCoord = chunkCoord - u_originSize.xy;

		if (localGridCoord.x < 0 || localGridCoord.x >= u_originSize.z ||
		    localGridCoord.y < 0 || localGridCoord.y >= u_originSize.z) {
			break; // Out of grid bounds
		}

		vec2  gridUV = (vec2(localGridCoord) + 0.5) / float(u_originSize.z);

		// Coarse Skip: Mip 3 covers 8x8 chunks (256x256 units if scale=1)
		float h_max3 = textureLod(u_maxHeightGrid, gridUV, 3.0).r;
		if (p.y > h_max3 + 1.0) {
			// Skip by a safe multiple of chunk size (e.g., 4 chunks)
			t += 4.0 * scaledChunkSize;
			continue;
		}

		// Fine Skip: Mip 1 covers 2x2 chunks (64x64 units if scale=1)
		float h_max1 = textureLod(u_maxHeightGrid, gridUV, 1.0).r;
		if (p.y > h_max1 + 0.5) {
			// Skip by one chunk size
			t += scaledChunkSize;
			continue;
		}

		// At LOD 0, check actual chunk height
		int slice = texelFetch(u_chunkGrid, localGridCoord, 0).r;
		if (slice >= 0) {
			vec2  uv_chunk = (p.xz - vec2(chunkCoord) * scaledChunkSize) / scaledChunkSize;
			float h = texture(u_heightmapArray, vec3(uv_chunk, float(slice))).r;
			if (p.y < h) {
				return 0.0; // Hit terrain!
			}

			closest = min(closest, 8.0*((p.y-h)/t));
		}

		// Step size at LOD 0: proportional to world scale for smoothness
		t += 2.0 * u_terrainParams.y;
	}

	return closest;
}


bool isPointInTerrainShadow(vec3 worldPos, vec3 normal, vec3 lightDir) {
	return terrainShadowCoverage(worldPos, normal, lightDir) <= 0.0;
}

int isPointInTerrainShadowDebug(vec3 worldPos, vec3 normal, vec3 lightDir) {
	if (u_originSize.w < 1) return -3; // Blue
	if (u_terrainParams.y <= 0.0) return -1; // Cyan
	if (u_terrainParams.x <= 0.0) return -4; // White (Invalid chunkSize)
	if (lightDir.y <= 0.0) return -2; // Orange-ish (Light below horizon)

	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;
	vec3 p_start = worldPos + normal * (0.2 * u_terrainParams.y) + lightDir * (0.1 * u_terrainParams.y);
	float t = 0.0;
	float maxDist = 1200.0 * u_terrainParams.y;

	int iter = 0;
	while (t < maxDist && iter < 80) {
		iter++;
		vec3  p = p_start + t * lightDir;
		vec2  gridPos = p.xz / scaledChunkSize;
		ivec2 chunkCoord = ivec2(floor(gridPos));
		ivec2 localGridCoord = chunkCoord - u_originSize.xy;

		if (localGridCoord.x < 0) return 11;
		if (localGridCoord.x >= u_originSize.z) return 12;
		if (localGridCoord.y < 0) return 13;
		if (localGridCoord.y >= u_originSize.z) return 14;

		if (u_originSize.z != 128) return 5; // White/Debug: Invalid grid size

		vec2  gridUV = (vec2(localGridCoord) + 0.5) / float(u_originSize.z);

		float h_max3 = textureLod(u_maxHeightGrid, gridUV, 3.0).r;
		if (p.y > h_max3 + 1.0) {
			t += 4.0 * scaledChunkSize;
			continue;
		}

		float h_max1 = textureLod(u_maxHeightGrid, gridUV, 1.0).r;
		if (p.y > h_max1 + 0.5) {
			t += scaledChunkSize;
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

		t += 2.0 * u_terrainParams.y;
	}

	return 0; // Miss (Green)
}

#endif
