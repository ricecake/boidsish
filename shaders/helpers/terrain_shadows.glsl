#ifndef TERRAIN_SHADOWS_GLSL
#define TERRAIN_SHADOWS_GLSL

#include "fast_noise.glsl"

layout(std140, binding = 8) uniform TerrainData {
	ivec4 u_originSize;    // x, y=z, z=size, w=isBound
	vec4  u_terrainParams; // x=chunkSize, y=worldScale
};

uniform isampler2D u_chunkGrid;
uniform sampler2D  u_maxHeightGrid;
// u_heightmapArray is bound to unit 13
uniform sampler2DArray u_heightmapArray;

float getTerrainHeight(vec2 worldXZ) {
	if (u_originSize.w < 1)
		return -10000.0;
	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;
	vec2  gridPos = worldXZ / scaledChunkSize;
	ivec2 chunkCoord = ivec2(floor(gridPos));
	ivec2 localGridCoord = chunkCoord - u_originSize.xy;

	if (localGridCoord.x < 0 || localGridCoord.x >= u_originSize.z || localGridCoord.y < 0 ||
	    localGridCoord.y >= u_originSize.z) {
		return -9999.0; // Debug value
	}

	int slice = texelFetch(u_chunkGrid, localGridCoord, 0).r;
	if (slice < 0)
		return -10000.0;

	vec2 uv = (worldXZ - vec2(chunkCoord) * scaledChunkSize) / scaledChunkSize;
	return texture(u_heightmapArray, vec3(uv, float(slice))).r;
}

float terrainShadowCoverage(vec3 worldPos, vec3 normal, vec3 lightDir) {
	if (u_originSize.w < 1)
		return 1.0;
	// lightDir is from fragment to light
	float sundownShadow = smoothstep(0.0, 0.02, lightDir.y);
	if (lightDir.y <= 0.02) {
		return sundownShadow;
	}

	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;

	// Better initial bias: move along normal and a bit along light direction.
	vec3  p_start = worldPos + normal * (0.2 * u_terrainParams.y) + lightDir * 1.5;
	float t = 0.0;
	float maxDist = 1200.0 * u_terrainParams.y;

	vec2  rayDir = vec2(lightDir.x, lightDir.z);
	vec2  stepDir = sign(rayDir);
	// Avoid division by zero
	vec2  safeRayDir = vec2(abs(rayDir.x) < 1e-6 ? 1e-6 : abs(rayDir.x), abs(rayDir.y) < 1e-6 ? 1e-6 : abs(rayDir.y));
	vec2  tDelta = scaledChunkSize / safeRayDir;

	vec2  gridPos = p_start.xz / scaledChunkSize;
	ivec2 currentChunk = ivec2(floor(gridPos));

	vec2 tMax;
	tMax.x = (stepDir.x > 0.0) ? (floor(gridPos.x) + 1.0 - gridPos.x) * tDelta.x : (gridPos.x - floor(gridPos.x)) * tDelta.x;
	tMax.y = (stepDir.y > 0.0) ? (floor(gridPos.y) + 1.0 - gridPos.y) * tDelta.y : (gridPos.y - floor(gridPos.y)) * tDelta.y;

	float closest = 1.0;
	int   iter = 0;

	while (t < maxDist && iter < 128) {
		iter++;

		ivec2 localGridCoord = currentChunk - u_originSize.xy;
		if (localGridCoord.x < 0 || localGridCoord.x >= u_originSize.z || localGridCoord.y < 0 ||
		    localGridCoord.y >= u_originSize.z) {
			break; // Out of grid bounds
		}

		float tNext = min(tMax.x, tMax.y);
		float tEnd = min(tNext, maxDist);

		vec2  gridUV = (vec2(localGridCoord) + 0.5) / float(u_originSize.z);

		// Hi-Z Skip: Mip 3 covers 8x8 chunks
		float h_max3 = textureLod(u_maxHeightGrid, gridUV, 3.0).r;
		float rayYAtT = p_start.y + t * lightDir.y;

		if (rayYAtT < h_max3 + 1.0) {
			// Check mip 1 (2x2 chunks)
			float h_max1 = textureLod(u_maxHeightGrid, gridUV, 1.0).r;
			if (rayYAtT < h_max1 + 0.5) {
				// At LOD 0, check actual chunk height
				float h_max0 = textureLod(u_maxHeightGrid, gridUV, 0.0).r;
				if (rayYAtT < h_max0 + 0.25) {
					int slice = texelFetch(u_chunkGrid, localGridCoord, 0).r;
					if (slice >= 0) {
						// Sub-march inside this chunk
						float subT = t;
						float subStep = 1.5 * u_terrainParams.y;
						while (subT < tEnd) {
							vec3  p = p_start + subT * lightDir;
							vec2  uv_chunk = (p.xz - vec2(currentChunk) * scaledChunkSize) / scaledChunkSize;
							float h = texture(u_heightmapArray, vec3(uv_chunk, float(slice))).r;
							if (p.y < h) {
								return 0.0; // Hit terrain!
							}
							closest = min(closest, 8.0 * ((p.y - h) / subT));
							subT += subStep;
						}
					}
				}
			}
		}

		t = tEnd;
		if (t >= maxDist)
			break;

		if (tMax.x < tMax.y) {
			tMax.x += tDelta.x;
			currentChunk.x += int(stepDir.x);
		} else {
			tMax.y += tDelta.y;
			currentChunk.y += int(stepDir.y);
		}
	}

	return closest;
}

bool isPointInTerrainShadow(vec3 worldPos, vec3 normal, vec3 lightDir) {
	return terrainShadowCoverage(worldPos, normal, lightDir) <= 0.0;
}

int isPointInTerrainShadowDebug(vec3 worldPos, vec3 normal, vec3 lightDir) {
	if (u_originSize.w < 1)
		return -3; // Blue
	if (u_terrainParams.y <= 0.0)
		return -1; // Cyan
	if (u_terrainParams.x <= 0.0)
		return -4; // White (Invalid chunkSize)
	if (lightDir.y <= 0.02)
		return -2; // Orange-ish (Light below horizon or too low)

	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;
	vec3  p_start = worldPos + normal * (0.2 * u_terrainParams.y) + lightDir * 1.5;
	float t = 0.0;
	float maxDist = 1200.0 * u_terrainParams.y;

	vec2  rayDir = vec2(lightDir.x, lightDir.z);
	vec2  stepDir = sign(rayDir);
	vec2  safeRayDir = vec2(abs(rayDir.x) < 1e-6 ? 1e-6 : abs(rayDir.x), abs(rayDir.y) < 1e-6 ? 1e-6 : abs(rayDir.y));
	vec2  tDelta = scaledChunkSize / safeRayDir;

	vec2  gridPos = p_start.xz / scaledChunkSize;
	ivec2 currentChunk = ivec2(floor(gridPos));

	vec2 tMax;
	tMax.x = (stepDir.x > 0.0) ? (floor(gridPos.x) + 1.0 - gridPos.x) * tDelta.x : (gridPos.x - floor(gridPos.x)) * tDelta.x;
	tMax.y = (stepDir.y > 0.0) ? (floor(gridPos.y) + 1.0 - gridPos.y) * tDelta.y : (gridPos.y - floor(gridPos.y)) * tDelta.y;

	int iter = 0;
	while (t < maxDist && iter < 128) {
		iter++;

		ivec2 localGridCoord = currentChunk - u_originSize.xy;
		if (localGridCoord.x < 0)
			return 11;
		if (localGridCoord.x >= u_originSize.z)
			return 12;
		if (localGridCoord.y < 0)
			return 13;
		if (localGridCoord.y >= u_originSize.z)
			return 14;

		if (u_originSize.z != 128)
			return 5;

		float tNext = min(tMax.x, tMax.y);
		float tEnd = min(tNext, maxDist);

		vec2  gridUV = (vec2(localGridCoord) + 0.5) / float(u_originSize.z);
		float h_max3 = textureLod(u_maxHeightGrid, gridUV, 3.0).r;
		float rayYAtT = p_start.y + t * lightDir.y;

		if (rayYAtT < h_max3 + 1.0) {
			float h_max1 = textureLod(u_maxHeightGrid, gridUV, 1.0).r;
			if (rayYAtT < h_max1 + 0.5) {
				float h_max0 = textureLod(u_maxHeightGrid, gridUV, 0.0).r;
				if (rayYAtT < h_max0 + 0.25) {
					int slice = texelFetch(u_chunkGrid, localGridCoord, 0).r;
					if (slice < 0) {
						return 2; // No slice (Yellow)
					}

					// Sub-march inside this chunk
					float subT = t;
					float subStep = 1.5 * u_terrainParams.y;
					while (subT < tEnd) {
						vec3  p = p_start + subT * lightDir;
						vec2  uv_chunk = (p.xz - vec2(currentChunk) * scaledChunkSize) / scaledChunkSize;
						float h = texture(u_heightmapArray, vec3(uv_chunk, float(slice))).r;
						if (p.y < h) {
							return 3; // Hit! (Magenta)
						}
						subT += subStep;
					}
				}
			}
		}

		t = tEnd;
		if (t >= maxDist)
			break;

		if (tMax.x < tMax.y) {
			tMax.x += tDelta.x;
			currentChunk.x += int(stepDir.x);
		} else {
			tMax.y += tDelta.y;
			currentChunk.y += int(stepDir.y);
		}
	}

	return 0; // Miss (Green)
}

#endif
