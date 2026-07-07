#ifndef TERRAIN_SHADOWS_GLSL
#define TERRAIN_SHADOWS_GLSL

#include "fast_noise.glsl"
#include "terrain_common.glsl"

#ifndef TERRAIN_SHADOW_MAP_DEFINED
#define TERRAIN_SHADOW_MAP_DEFINED
uniform sampler2D u_terrainShadowMap;
#endif

#ifndef TERRAIN_HORIZON_MAP_DEFINED
#define TERRAIN_HORIZON_MAP_DEFINED
layout(binding = [[TERRAIN_HORIZON_MAP_BINDING]]) uniform sampler2DArray u_terrainHorizonMap;
#endif

/**
 * Calculate visibility based on precomputed horizon angles for a chunk.
 */
float calculateHorizonVisibility(vec3 worldPos, vec3 lightDir) {
	if (u_originSize.w < 1) return 1.0;

	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;
	vec2  gridPos = worldPos.xz / scaledChunkSize;
	ivec2 chunkCoord = ivec2(floor(gridPos));
	ivec2 localGridCoord = chunkCoord - u_originSize.xy;

	if (localGridCoord.x < 0 || localGridCoord.x >= u_originSize.z ||
		localGridCoord.y < 0 || localGridCoord.y >= u_originSize.z) {
		return 1.0;
	}

	int slice = int(texelFetch(u_chunkGrid, localGridCoord, 0).r);
	if (slice < 0) return 1.0;

	vec2 uv_chunk = (worldPos.xz - vec2(chunkCoord) * scaledChunkSize) / scaledChunkSize;
	vec4 horizons = texture(u_terrainHorizonMap, vec3(uv_chunk, float(slice)));

	vec2 dirXZ = normalize(lightDir.xz);
	float sunElevation = atan(lightDir.y / max(1e-6, length(lightDir.xz)));

	float horizonAngle = 0.0;
	float wE = max(0.0, dirXZ.x);
	float wN = max(0.0, dirXZ.y);
	float wW = max(0.0, -dirXZ.x);
	float wS = max(0.0, -dirXZ.y);
	float totalW = wE + wN + wW + wS;
	if (totalW > 1e-6) {
		horizonAngle = (wE * horizons.x + wN * horizons.y + wW * horizons.z + wS * horizons.w) / totalW;
	}

	const float margin = 0.1;
	if (sunElevation > horizonAngle + margin) return 1.0;
	if (sunElevation < horizonAngle - margin) return 0.0;

	return smoothstep(horizonAngle - margin, horizonAngle + margin, sunElevation);
}

/**
 * Internal unified hierarchical Hi-Z raymarcher.
 * Efficiently traverses both grid-level and chunk-level min-max hierarchies.
 */
float marchTerrainHierarchical(vec3 p_start, vec3 rayDir, float maxDist, bool softShadow) {
	float t = 0.0;
	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;
	float worldScaleVal = u_terrainParams.y;
	float rawChunkSize = u_terrainParams.x;

	vec2 rayDirXZ = vec2(rayDir.x, rayDir.z);
	vec2 stepDir = sign(rayDirXZ);
	vec2 safeRayDir = vec2(abs(rayDirXZ.x) < 1e-6 ? 1e-6 : abs(rayDirXZ.x), abs(rayDirXZ.y) < 1e-6 ? 1e-6 : abs(rayDirXZ.y));
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
	const int MAX_GRID_ITER = 64;

	while (t < maxDist && iter++ < MAX_GRID_ITER) {
		ivec2 localGridCoord = currentChunk - u_originSize.xy;
		if (localGridCoord.x < 0 || localGridCoord.x >= u_originSize.z || localGridCoord.y < 0 || localGridCoord.y >= u_originSize.z) {
			break;
		}

		float tNext = min(tMax.x, tMax.y);
		float tEnd = min(tNext, maxDist);

		// --- GRID LEVEL HI-Z ---
		// Fetch slice and max height from consolidated grid texture
		vec2 gridData = texelFetch(u_chunkGrid, localGridCoord, 0).rg;
		int slice = int(gridData.x);
		float hMaxChunk = gridData.y;

		float rayYNear = p_start.y + t * rayDir.y;
		float rayYFar  = p_start.y + tEnd * rayDir.y;
		float rayYMin = min(rayYNear, rayYFar);

		if (slice >= 0 && rayYMin < hMaxChunk) {
			// --- CHUNK LEVEL HI-Z ---
			// Hierarchical raymarch inside the chunk using min-max height mips
			float subT = t;
			const int MAX_CHUNK_ITER = 32;
			int subIter = 0;

			// Start at a reasonable mip level for the chunk (e.g. mip 3 for 32x32 heightmap)
			int mip = 3;
			float resAtMip = rawChunkSize / float(1 << mip);
			float subStepSize = (scaledChunkSize / resAtMip);

			while (subT < tEnd && subIter++ < MAX_CHUNK_ITER) {
				vec3 p = p_start + subT * rayDir;
				vec2 uv_chunk = (p.xz - vec2(currentChunk) * scaledChunkSize) / scaledChunkSize;

				// Sample min-max heights from u_heightmapArray mips
				// RG: R=max, G=min
				vec2 minMax = textureLod(u_heightmapArray, vec3(uv_chunk, float(slice)), float(mip)).rg;

				if (p.y < minMax.x) {
					if (mip > 0) {
						// Refine: descend hierarchy
						mip--;
						resAtMip = rawChunkSize / float(1 << mip);
						subStepSize = (scaledChunkSize / resAtMip);
						continue;
					} else {
						// At base level, check for hit
						float h = minMax.x;
						if (p.y < h) {
							if (!softShadow) return 0.0;
							closest = min(closest, 8.0 * ((p.y - h) / max(0.01, subT)));
							if (closest <= 0.0) return 0.0;
						}
						subT += 0.5 * worldScaleVal; // Small fixed step at LOD 0
					}
				} else {
					// Skip: advance to next cell at current mip
					subT += subStepSize;
					// Optionally ascend hierarchy
					if (mip < 3) mip++;
				}
			}
		}

		t = tEnd;
		if (tMax.x < tMax.y) {
			tMax.x += tDelta.x;
			currentChunk.x += int(stepDir.x);
		} else {
			tMax.y += tDelta.y;
			currentChunk.y += int(stepDir.y);
		}
	}

	return softShadow ? clamp(closest, 0.0, 1.0) : 1.0;
}

/**
 * Perform a hierarchical raymarch in a specific direction to check for terrain occlusion.
 */
float marchOcclusion(vec3 p_start, vec3 rayDir, float maxDist) {
	return marchTerrainHierarchical(p_start + rayDir * (2.0 * u_terrainParams.y), rayDir, maxDist, true);
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

	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;

#ifndef SKIP_SHADOW_MAP_LOOKUP
	// Fast path: Check precomputed terrain data
	float h_surface = getTerrainHeight(worldPos.xz);
	if (abs(worldPos.y - h_surface) < (u_terrainParams.y)) {
		float shadowMapWorldSize = float(u_originSize.z) * scaledChunkSize;
		vec2  shadowOrigin = vec2(u_originSize.xy) * scaledChunkSize;
		vec2  shadowUV = (worldPos.xz - shadowOrigin) / shadowMapWorldSize;

		if (shadowUV.x >= 0.0 && shadowUV.x <= 1.0 && shadowUV.y >= 0.0 && shadowUV.y <= 1.0) {
			return texture(u_terrainShadowMap, shadowUV).r;
		}
	} else {
		if (calculateHorizonVisibility(worldPos, lightDir) >= 0.99) {
			return 1.0;
		}
	}
#endif

	float sundownShadow = smoothstep(0.0, 0.02, lightDir.y);
	if (lightDir.y <= 0.02) {
		return sundownShadow;
	}

	vec3  p_start = worldPos + normal * (0.8 * u_terrainParams.y) + lightDir * (1.2 * u_terrainParams.y);
	return marchTerrainHierarchical(p_start, lightDir, 1200.0 * u_terrainParams.y, true);
}

/**
 * Terrain shadow for volumetric voxels in free space.
 * Bypasses the 2D shadow map and horizon early-outs (which are ground-relative)
 * and goes straight to the DDA march with Hi-Z acceleration.
 * Returns soft shadow: 0.0 = fully occluded, 1.0 = fully lit.
 */
float terrainShadowVolumetric(vec3 worldPos, vec3 lightDir) {
	if (u_originSize.w < 1)
		return 1.0;

	float sundownShadow = smoothstep(0.0, 0.02, lightDir.y);
	if (lightDir.y <= 0.02) {
		return sundownShadow;
	}

	vec3  p_start = worldPos + lightDir * (1.0 * u_terrainParams.y);
	return marchTerrainHierarchical(p_start, lightDir, 800.0 * u_terrainParams.y, true);
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

	vec3  p_start = worldPos + normal * (0.8 * u_terrainParams.y) + lightDir * (1.2 * u_terrainParams.y);
	float shadow = marchTerrainHierarchical(p_start, lightDir, 1200.0 * u_terrainParams.y, false);
	return shadow < 1.0 ? 3 : 0;
}

#endif
