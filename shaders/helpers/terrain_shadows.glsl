#ifndef TERRAIN_SHADOWS_GLSL
#define TERRAIN_SHADOWS_GLSL

#include "fast_noise.glsl"
#include "terrain_common.glsl"

#ifndef TERRAIN_SHADOW_MAP_DEFINED
#define TERRAIN_SHADOW_MAP_DEFINED
uniform sampler2D u_terrainShadowMap;
#endif

/**
 * Perform a coarse raymarch in a specific direction to check for terrain occlusion.
 * Similar to terrainShadowCoverage but optimized for ambient occlusion (AO).
 */
float marchOcclusion(vec3 p_start, vec3 rayDir, float maxDist) {
	float t = 5.0 * u_terrainParams.y; // Start a bit away from surface
	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;

	float visibility = 1.0;

	float stepCount = 0;
	float maxSteps = 8;
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

		// Safe hierarchical skip: if we're well above the maximum terrain height in this area,
		// we can skip ahead.
		float h_max3 = textureLod(u_maxHeightGrid, gridUV, 3.0).r;
		if (p.y > h_max3 + (10.0 * u_terrainParams.y)) {
			float distToMax = (p.y - h_max3);
			if (rayDir.y < 0.0) {
				// Marching downwards: only skip as far as it takes to reach h_max
				t += min(step, distToMax / abs(rayDir.y));
			} else {
				// Marching upwards or horizontal: skip a full chunk
				t += scaledChunkSize;
			}
			continue;
		}

		float h_max = textureLod(u_maxHeightGrid, gridUV, 0.0).r;

		if (p.y < h_max) {
			int slice = texelFetch(u_chunkGrid, localGridCoord, 0).r;
			if (slice >= 0) {
				vec2  uv_chunk = (p.xz - vec2(chunkCoord) * scaledChunkSize) / scaledChunkSize;
				vec2  remappedUV = (uv_chunk * u_terrainParams.x + 0.5) / (u_terrainParams.x + 1.0);
				float h = texture(u_heightmapArray, vec3(remappedUV, float(slice))).r;

				if (p.y < h) {
					// Soft occlusion based on how much terrain is "above" the ray point
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

	float ao = occ / 6.0;
	ao = pow(ao, 1.5);
	return clamp(ao + normal.y * 0.15, 0.0, 1.0);
}

float terrainShadowCoverage(vec3 worldPos, vec3 normal, vec3 lightDir) {
	if (u_originSize.w < 1)
		return 1.0;

	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;

#ifndef SKIP_SHADOW_MAP_LOOKUP
	// Fast path: Precomputed terrain shadow map
	float shadowMapWorldSize = float(u_originSize.z) * scaledChunkSize;
	vec2 shadowOrigin = vec2(u_originSize.xy) * scaledChunkSize;
	vec2 shadowUV = (worldPos.xz - shadowOrigin) / shadowMapWorldSize;

	if (shadowUV.x >= 0.0 && shadowUV.x <= 1.0 && shadowUV.y >= 0.0 && shadowUV.y <= 1.0) {
		return texture(u_terrainShadowMap, shadowUV).r;
	}
#endif

	float sundownShadow = smoothstep(0.0, 0.02, lightDir.y);
	if (lightDir.y <= 0.02) {
		return sundownShadow;
	}

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

	float closest = 1.0;
	int   iter = 0;

	while (t < maxDist && iter < 128) {
		iter++;

		ivec2 localGridCoord = currentChunk - u_originSize.xy;
		if (localGridCoord.x < 0 || localGridCoord.x >= u_originSize.z || localGridCoord.y < 0 ||
		    localGridCoord.y >= u_originSize.z) {
			break;
		}

		float tNext = min(tMax.x, tMax.y);
		float tEnd = min(tNext, maxDist);

		vec2 gridUV = (vec2(localGridCoord) + 0.5) / float(u_originSize.z);

		// Hi-Z Skip: using max height mips to skip expensive sub-marches
		float h_max3 = textureLod(u_maxHeightGrid, gridUV, 3.0).r;
		float rayYAtT = p_start.y + t * lightDir.y;

		if (rayYAtT < h_max3 + (3.0 * u_terrainParams.y)) {
			float h_max1 = textureLod(u_maxHeightGrid, gridUV, 1.0).r;
			if (rayYAtT < h_max1 + (2.0 * u_terrainParams.y)) {
				float h_max0 = textureLod(u_maxHeightGrid, gridUV, 0.0).r;
				if (rayYAtT < h_max0 + (1.0 * u_terrainParams.y)) {
					int slice = texelFetch(u_chunkGrid, localGridCoord, 0).r;
					if (slice >= 0) {
						float subT = t;
						float subStep = 0.5 * u_terrainParams.y;
						while (subT < tEnd) {
							vec3  p = p_start + subT * lightDir;
							vec2  uv_chunk = (p.xz - vec2(currentChunk) * scaledChunkSize) / scaledChunkSize;
							vec2  remappedUV = (uv_chunk * u_terrainParams.x + 0.5) / (u_terrainParams.x + 1.0);
							float h = texture(u_heightmapArray, vec3(remappedUV, float(slice))).r;
							closest = min(closest, 8.0 * ((p.y - h) / subT));
							if (p.y < h) {
								return clamp(subT / (maxDist), 0.0, 1.0);
							}
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

	return 1.0-clamp(pow(closest, 3) / (maxDist), 0.0, 1.0);
}

bool isPointInTerrainShadow(vec3 worldPos, vec3 normal, vec3 lightDir) {
	return terrainShadowCoverage(worldPos, normal, lightDir) <= 0.0;
}

int isPointInTerrainShadowDebug(vec3 worldPos, vec3 normal, vec3 lightDir) {
	if (u_originSize.w < 1) return -3;
	if (u_terrainParams.y <= 0.0) return -1;
	if (lightDir.y <= 0.02) return -2;

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
	tMax.x = (stepDir.x > 0.0) ? (floor(gridPos.x) + 1.0 - gridPos.x) * tDelta.x : (gridPos.x - floor(gridPos.x)) * tDelta.x;
	tMax.y = (stepDir.y > 0.0) ? (floor(gridPos.y) + 1.0 - gridPos.y) * tDelta.y : (gridPos.y - floor(gridPos.y)) * tDelta.y;

	int iter = 0;
	while (t < maxDist && iter < 128) {
		iter++;
		ivec2 localGridCoord = currentChunk - u_originSize.xy;
		if (localGridCoord.x < 0 || localGridCoord.x >= u_originSize.z || localGridCoord.y < 0 || localGridCoord.y >= u_originSize.z) return 11;

		float tNext = min(tMax.x, tMax.y);
		float tEnd = min(tNext, maxDist);

		vec2  gridUV = (vec2(localGridCoord) + 0.5) / float(u_originSize.z);
		float h_max3 = textureLod(u_maxHeightGrid, gridUV, 3.0).r;
		float rayYAtT = p_start.y + t * lightDir.y;

		if (rayYAtT < h_max3 + (3.0 * u_terrainParams.y)) {
			int slice = texelFetch(u_chunkGrid, localGridCoord, 0).r;
			if (slice >= 0) {
				float subT = t;
				float subStep = 0.5 * u_terrainParams.y;
				while (subT < tEnd) {
					vec3  p = p_start + subT * lightDir;
					vec2  uv_chunk = (p.xz - vec2(currentChunk) * scaledChunkSize) / scaledChunkSize;
					vec2  remappedUV = (uv_chunk * u_terrainParams.x + 0.5) / (u_terrainParams.x + 1.0);
					float h = texture(u_heightmapArray, vec3(remappedUV, float(slice))).r;
					if (p.y < h) return 3;
					subT += subStep;
				}
			}
		}

		t = tEnd;
		if (tMax.x < tMax.y) { tMax.x += tDelta.x; currentChunk.x += int(stepDir.x); }
		else { tMax.y += tDelta.y; currentChunk.y += int(stepDir.y); }
	}
	return 0;
}

#endif
