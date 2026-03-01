#ifndef TERRAIN_SHADOWS_GLSL
#define TERRAIN_SHADOWS_GLSL

layout(std140) uniform TerrainData {
	ivec2 u_gridOrigin;
	int   u_gridSize;
	float u_chunkSize;
	float u_worldScale;
};

uniform isampler2D    u_chunkGrid;
uniform sampler2D     u_maxHeightGrid;
// u_heightmapArray is bound to unit 13
uniform sampler2DArray u_heightmapArray;

float getTerrainHeight(vec2 worldXZ) {
	if (u_worldScale <= 0.0) return -10000.0;
	float scaledChunkSize = u_chunkSize * u_worldScale;
	ivec2 chunkCoord = ivec2(floor(worldXZ / scaledChunkSize));
	ivec2 localGridCoord = chunkCoord - u_gridOrigin;

	if (localGridCoord.x < 0 || localGridCoord.x >= u_gridSize ||
	    localGridCoord.y < 0 || localGridCoord.y >= u_gridSize) {
		return -10000.0;
	}

	int slice = texelFetch(u_chunkGrid, localGridCoord, 0).r;
	if (slice < 0)
		return -10000.0;

	vec2 uv = (worldXZ - vec2(chunkCoord) * scaledChunkSize) / scaledChunkSize;
	return texture(u_heightmapArray, vec3(uv, float(slice))).r;
}

bool isPointInTerrainShadow(vec3 worldPos, vec3 lightDir) {
	if (u_worldScale <= 0.0) return false;
	// lightDir is from fragment to light
	if (lightDir.y <= 0.0)
		return false;

	float scaledChunkSize = u_chunkSize * u_worldScale;
	// Use a small bias to avoid self-shadowing acne
	float t = 0.2 * u_worldScale;
	float maxDist = 800.0 * u_worldScale;

	// Avoid division by zero
	vec3 safeInvDir = 1.0 / (abs(lightDir) + vec3(1e-6));
	safeInvDir *= sign(lightDir);

	int iter = 0;
	while (t < maxDist && iter < 64) {
		iter++;
		vec3  p = worldPos + t * lightDir;
		ivec2 chunkCoord = ivec2(floor(p.xz / scaledChunkSize));
		ivec2 localGridCoord = chunkCoord - u_gridOrigin;

		if (localGridCoord.x < 0 || localGridCoord.x >= u_gridSize ||
		    localGridCoord.y < 0 || localGridCoord.y >= u_gridSize) {
			break; // Out of grid bounds
		}

		bool skipped = false;
		// Hierarchical skipping using Max Height Mips
		for (int lod = 5; lod >= 1; --lod) {
			int   levelSize = 1 << lod;
			ivec2 lodGridCoord = localGridCoord / levelSize;
			float maxHeight = textureLod(u_maxHeightGrid, (vec2(lodGridCoord) + 0.5) / float(u_gridSize >> lod), float(lod)).r;

			if (p.y > maxHeight) {
				// Ray is above max height of this large region and moving up.
				// Skip to the edge of this region.
				vec2 regionMin = vec2(u_gridOrigin + lodGridCoord * levelSize) * scaledChunkSize;
				vec2 regionMax = regionMin + vec2(levelSize) * scaledChunkSize;

				// Distance to exit region boundaries
				vec2 planes = mix(regionMin, regionMax, step(0.0, lightDir.xz));
				vec2 tExitXZ = (planes - worldPos.xz) * safeInvDir.xz;
				float tExit = min(tExitXZ.x, tExitXZ.y);

				if (tExit > t) {
					t = tExit + 0.05 * u_worldScale;
					skipped = true;
				}
				break;
			}
		}

		if (!skipped) {
			// At LOD 0 (or couldn't skip), check actual chunk height
			int slice = texelFetch(u_chunkGrid, localGridCoord, 0).r;
			if (slice >= 0) {
				vec2  uv = (p.xz - vec2(chunkCoord) * scaledChunkSize) / scaledChunkSize;
				float h = texture(u_heightmapArray, vec3(uv, float(slice))).r;
				if (p.y < h)
					return true; // Hit terrain!
			}

			// Traditional ray march step for the detailed part
			t += 2.0 * u_worldScale;
		}
	}

	return false;
}

#endif
