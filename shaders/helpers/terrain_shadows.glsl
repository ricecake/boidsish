#ifndef TERRAIN_SHADOWS_GLSL
#define TERRAIN_SHADOWS_GLSL

#include "fast_noise.glsl"

#ifndef TERRAIN_DATA_BLOCK
#define TERRAIN_DATA_BLOCK
layout(std140, binding = [[TERRAIN_DATA_BINDING]]) uniform TerrainData {
	ivec4 u_originSize;    // x, y=z, z=size, w=isBound
	vec4  u_terrainParams; // x=chunkSize, y=worldScale
};
#endif

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
	vec2 remappedUV = (uv * u_terrainParams.x + 0.5) / (u_terrainParams.x + 1.0);
	return texture(u_heightmapArray, vec3(remappedUV, float(slice))).r;
}

/**
 * Perform a coarse raymarch in a specific direction to check for terrain occlusion.
 * Similar to terrainShadowCoverage but optimized for ambient occlusion (AO).
 */
float marchOcclusion(vec3 p_start, vec3 rayDir, float maxDist) {
	float t = 5.0 * u_terrainParams.y; // Start a bit away from surface
	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;

	// Coarse step size for AO march
	// float step = 12.0 * u_terrainParams.y;
	float visibility = 1.0;

	float stepCount = 0;
	float maxSteps = 5;
	float step = maxDist/maxSteps;
	while (t < maxDist && stepCount++ <= maxSteps) {
		vec3  p = p_start + t * rayDir;
		vec2  gridPos = p.xz / scaledChunkSize;
		ivec2 chunkCoord = ivec2(floor(gridPos));
		ivec2 localGridCoord = chunkCoord - u_originSize.xy;

		if (localGridCoord.x < 0 || localGridCoord.x >= u_originSize.z || localGridCoord.y < 0 ||
		    localGridCoord.y >= u_originSize.z) {
			break;
		}

		vec2  gridUV = (vec2(localGridCoord) + 0.5) / float(u_originSize.z);
		float h_max = textureLod(u_maxHeightGrid, gridUV, 0.0).r;

		if (p.y < h_max) {
			int slice = texelFetch(u_chunkGrid, localGridCoord, 0).r;
			if (slice >= 0) {
				vec2  uv_chunk = (p.xz - vec2(chunkCoord) * scaledChunkSize) / scaledChunkSize;
				vec2  remappedUV = (uv_chunk * u_terrainParams.x + 0.5) / (u_terrainParams.x + 1.0);
				float h = texture(u_heightmapArray, vec3(remappedUV, float(slice))).r;

				if (p.y < h) {
					// Soft occlusion based on how much terrain is "above" the ray point
					// This eliminates blocky artifacts from constant max-height values
					float h_diff = h - p.y;
					visibility = min(visibility, clamp(1.0 - h_diff / (20.0 * u_terrainParams.y), 0.0, 1.0));

					if (visibility <= 0.0)
						return 0.0;
				}
			}
		}

		t += step;
	}

	return visibility;
}

/**
 * Calculate macro terrain occlusion by sampling in 6 directions around the horizon.
 * Returns [0, 1] where 0 is fully occluded (valley) and 1 is open sky.
 */
float calculateTerrainOcclusion(vec3 worldPos, vec3 normal) {
	if (u_originSize.w < 1)
		return 1.0;

	// Use 6 directions for better horizon coverage
	// Samples at ~30 degrees elevation to capture nearby peaks
	const float h = 0.866; // cos(30)
	const float v = 0.5;   // sin(30)

	vec3 dirs[6] = {
		vec3(h, v, 0.0),
		vec3(-h, v, 0.0),
		vec3(h * 0.5, v, h),
		vec3(-h * 0.5, v, h),
		vec3(h * 0.5, v, -h),
		vec3(-h * 0.5, v, -h)
	};

	float occ = 0.0;
	float maxDist = 50.0 * u_terrainParams.y;
	vec3  p_start = worldPos + normal * (1.5 * u_terrainParams.y); // Lift off surface

	for (int i = 0; i < 6; ++i) {
		occ += marchOcclusion(p_start, dirs[i], maxDist);
	}

	// Ambient Occlusion is the average visibility
	float ao = occ / 6.0;

	// Apply a stronger curve to valleys to increase contrast
	ao = pow(ao, 1.5);

	// Boost for flat/upward surfaces
	return clamp(ao + normal.y * 0.15, 0.0, 1.0);
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
	// Increased bias and light-dir push to prevent self-shadowing grooves at chunk boundaries.
	vec3  p_start = worldPos + normal * (0.8 * u_terrainParams.y) + lightDir * (1.2 * u_terrainParams.y);
	float t = 0.0;
	float maxDist = 1200.0 * u_terrainParams.y;

	vec2 rayDir = vec2(lightDir.x, lightDir.z);
	vec2 stepDir = sign(rayDir);
	// Avoid division by zero
	vec2 safeRayDir = vec2(abs(rayDir.x) < 1e-6 ? 1e-6 : abs(rayDir.x), abs(rayDir.y) < 1e-6 ? 1e-6 : abs(rayDir.y));
	vec2 tDelta = scaledChunkSize / safeRayDir;

	vec2  gridPos = p_start.xz / scaledChunkSize;
	ivec2 currentChunk = ivec2(floor(gridPos));

	vec2 tMax;
	tMax.x = (stepDir.x > 0.0) ? (floor(gridPos.x) + 1.0 - gridPos.x) * tDelta.x
							   : (gridPos.x - floor(gridPos.x)) * tDelta.x;
	tMax.y = (stepDir.y > 0.0) ? (floor(gridPos.y) + 1.0 - gridPos.y) * tDelta.y
							   : (gridPos.y - floor(gridPos.y)) * tDelta.y;

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

		vec2 gridUV = (vec2(localGridCoord) + 0.5) / float(u_originSize.z);

		// Hi-Z Skip: Mip 3 covers 8x8 chunks
		float h_max3 = textureLod(u_maxHeightGrid, gridUV, 3.0).r;
		float rayYAtT = p_start.y + t * lightDir.y;

		if (rayYAtT < h_max3 + (2.0 * u_terrainParams.y)) {
			// Check mip 1 (2x2 chunks)
			float h_max1 = textureLod(u_maxHeightGrid, gridUV, 1.0).r;
			if (rayYAtT < h_max1 + (1.0 * u_terrainParams.y)) {
				// At LOD 0, check actual chunk height
				float h_max0 = textureLod(u_maxHeightGrid, gridUV, 0.0).r;
				if (rayYAtT < h_max0 + (0.5 * u_terrainParams.y)) {
					int slice = texelFetch(u_chunkGrid, localGridCoord, 0).r;
					if (slice >= 0) {
						// Sub-march inside this chunk
						float subT = t;
						float subStep = 0.5 * u_terrainParams.y;
						while (subT < tEnd) {
							vec3  p = p_start + subT * lightDir;
							vec2  uv_chunk = (p.xz - vec2(currentChunk) * scaledChunkSize) / scaledChunkSize;
							vec2  remappedUV = (uv_chunk * u_terrainParams.x + 0.5) / (u_terrainParams.x + 1.0);
							float h = texture(u_heightmapArray, vec3(remappedUV, float(slice))).r;
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
	vec3  p_start = worldPos + normal * (0.8 * u_terrainParams.y) + lightDir * (1.2 * u_terrainParams.y);
	float t = 0.0;
	float maxDist = 1200.0 * u_terrainParams.y;

	vec2 rayDir = vec2(lightDir.x, lightDir.z);
	vec2 stepDir = sign(rayDir);
	vec2 safeRayDir = vec2(abs(rayDir.x) < 1e-6 ? 1e-6 : abs(rayDir.x), abs(rayDir.y) < 1e-6 ? 1e-6 : abs(rayDir.y));
	vec2 tDelta = scaledChunkSize / safeRayDir;

	vec2  gridPos = p_start.xz / scaledChunkSize;
	ivec2 currentChunk = ivec2(floor(gridPos));

	vec2 tMax;
	tMax.x = (stepDir.x > 0.0) ? (floor(gridPos.x) + 1.0 - gridPos.x) * tDelta.x
							   : (gridPos.x - floor(gridPos.x)) * tDelta.x;
	tMax.y = (stepDir.y > 0.0) ? (floor(gridPos.y) + 1.0 - gridPos.y) * tDelta.y
							   : (gridPos.y - floor(gridPos.y)) * tDelta.y;

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

		float tNext = min(tMax.x, tMax.y);
		float tEnd = min(tNext, maxDist);

		vec2  gridUV = (vec2(localGridCoord) + 0.5) / float(u_originSize.z);
		float h_max3 = textureLod(u_maxHeightGrid, gridUV, 3.0).r;
		float rayYAtT = p_start.y + t * lightDir.y;

		if (rayYAtT < h_max3 + (2.0 * u_terrainParams.y)) {
			float h_max1 = textureLod(u_maxHeightGrid, gridUV, 1.0).r;
			if (rayYAtT < h_max1 + (1.0 * u_terrainParams.y)) {
				float h_max0 = textureLod(u_maxHeightGrid, gridUV, 0.0).r;
				if (rayYAtT < h_max0 + (0.5 * u_terrainParams.y)) {
					int slice = texelFetch(u_chunkGrid, localGridCoord, 0).r;
					if (slice < 0) {
						return 2; // No slice (Yellow)
					}

					// Sub-march inside this chunk
					float subT = t;
					float subStep = 0.5 * u_terrainParams.y;
					while (subT < tEnd) {
						vec3  p = p_start + subT * lightDir;
						vec2  uv_chunk = (p.xz - vec2(currentChunk) * scaledChunkSize) / scaledChunkSize;
						vec2  remappedUV = (uv_chunk * u_terrainParams.x + 0.5) / (u_terrainParams.x + 1.0);
						float h = texture(u_heightmapArray, vec3(remappedUV, float(slice))).r;
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
