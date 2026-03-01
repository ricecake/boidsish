#ifndef TERRAIN_SHADOWS_GLSL
#define TERRAIN_SHADOWS_GLSL

layout(std140) uniform TerrainData {
	ivec4 u_originSize;
	vec4  u_terrainParams;
};

uniform isampler2D    u_chunkGrid;
uniform sampler2D     u_maxHeightGrid;
// u_heightmapArray is bound to unit 13
uniform sampler2DArray u_heightmapArray;

float getTerrainHeight(vec2 worldXZ) {
	if (u_terrainParams.y <= 0.0) return -10000.0;
	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;
	ivec2 chunkCoord = ivec2(floor(worldXZ / scaledChunkSize));
	ivec2 localGridCoord = chunkCoord - u_originSize.xz;

	if (localGridCoord.x < 0 || localGridCoord.x >= u_originSize.z ||
	    localGridCoord.y < 0 || localGridCoord.y >= u_originSize.z) {
		return -10000.0;
	}

	int slice = texelFetch(u_chunkGrid, localGridCoord, 0).r;
	if (slice < 0)
		return -10000.0;

	vec2 uv = (worldXZ - vec2(chunkCoord) * scaledChunkSize) / scaledChunkSize;
	return texture(u_heightmapArray, vec3(uv, float(slice))).r;
}

bool isPointInTerrainShadow(vec3 worldPos, vec3 lightDir) {
	if (u_terrainParams.y <= 0.0) return false;
	// lightDir is from fragment to light
	if (lightDir.y <= 0.0)
		return false;

	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;
	// Use a small bias to avoid self-shadowing acne
	float t = 0.2 * u_terrainParams.y;
	float maxDist = 800.0 * u_terrainParams.y;

	// Avoid division by zero
	vec3 safeInvDir = 1.0 / (abs(lightDir) + vec3(1e-6));
	safeInvDir *= sign(lightDir);

	int iter = 0;
	while (t < maxDist && iter < 64) {
		iter++;
		vec3  p = worldPos + t * lightDir;
		ivec2 chunkCoord = ivec2(floor(p.xz / scaledChunkSize));
		ivec2 localGridCoord = chunkCoord - u_originSize.xz;

		if (localGridCoord.x < 0 || localGridCoord.x >= u_originSize.z ||
		    localGridCoord.y < 0 || localGridCoord.y >= u_originSize.z) {
			break; // Out of grid bounds
		}

		// Check actual chunk height
		int slice = texelFetch(u_chunkGrid, localGridCoord, 0).r;
		if (slice >= 0) {
			vec2  uv = (p.xz - vec2(chunkCoord) * scaledChunkSize) / scaledChunkSize;
			float h = texture(u_heightmapArray, vec3(uv, float(slice))).r;
			if (p.y < h)
				return true; // Hit terrain!
		}

		// Progress ray
		t += 2.0 * u_terrainParams.y;
	}

	return false;
}

int isPointInTerrainShadowDebug(vec3 worldPos, vec3 lightDir) {
	if (u_terrainParams.y <= 0.0) return -1;
	if (lightDir.y <= 0.0) return -2;

	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;
	float t = 0.2 * u_terrainParams.y;
	float maxDist = 800.0 * u_terrainParams.y;

	int iter = 0;
	while (t < maxDist && iter < 64) {
		iter++;
		vec3  p = worldPos + t * lightDir;
		ivec2 chunkCoord = ivec2(floor(p.xz / scaledChunkSize));
		ivec2 localGridCoord = chunkCoord - u_originSize.xz;

		if (localGridCoord.x < 0 || localGridCoord.x >= u_originSize.z ||
		    localGridCoord.y < 0 || localGridCoord.y >= u_originSize.z) {
			return 1; // Out of bounds
		}

		int slice = texelFetch(u_chunkGrid, localGridCoord, 0).r;
		if (slice < 0) {
			return 2; // No slice
		}

		vec2  uv = (p.xz - vec2(chunkCoord) * scaledChunkSize) / scaledChunkSize;
		float h = texture(u_heightmapArray, vec3(uv, float(slice))).r;
		if (p.y < h)
			return 3; // Hit!

		t += 4.0 * u_terrainParams.y;
	}

	return 0; // Miss
}

#endif
