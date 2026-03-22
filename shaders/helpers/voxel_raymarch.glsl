#ifndef VOXEL_RAYMARCH_GLSL
#define VOXEL_RAYMARCH_GLSL

#include "voxel_brick.glsl"

float sample_voxel_pool(vec3 worldPos, ivec3 gPos, int poolIdx, sampler3D poolTex) {
    float brick_world_size = float(VOXEL_BRICK_SIZE) * VOXEL_SIZE;
    vec3 localPos = (worldPos - (vec3(gPos) * brick_world_size)) / brick_world_size;

    // Clamp localPos to [0, 1] for safety
    localPos = clamp(localPos, 0.0, 1.0);

    vec3 uv = pool_index_to_uv(poolIdx, localPos);
    return texture(poolTex, uv).r;
}

float sample_voxel_density(vec3 worldPos, sampler3D poolTex) {
    ivec3 gPos = world_to_grid(worldPos);
    int poolIdx = lookup_brick(gPos);
    if (poolIdx == -1) return 0.0;
    return sample_voxel_pool(worldPos, gPos, poolIdx, poolTex);
}

// Simple raymarcher that skips empty bricks
// Returns accumulated density and total distance
vec4 raymarch_density(vec3 ro, vec3 rd, float maxDist, sampler3D poolTex) {
    float brick_world_size = float(VOXEL_BRICK_SIZE) * VOXEL_SIZE;
    float step_size = VOXEL_SIZE * 1.0;

    float accum_density = 0.0;
    float t = 0.0;

    // Grid traversal with brick skipping
    for (int i = 0; i < 512 && t < maxDist; i++) {
        vec3 p = ro + rd * t;
        ivec3 gPos = world_to_grid(p);
        int poolIdx = lookup_brick(gPos);

        // Calculate distance to leave this brick along rd
        vec3 brickCorner = vec3(gPos) * brick_world_size;
        vec3 exit = (brickCorner + brick_world_size * step(0.0, rd));
        vec3 distToExit = (exit - p) / (rd + vec3(1e-7));
        float dtNextBrick = min(min(distToExit.x, distToExit.y), distToExit.z);

        if (poolIdx == -1) {
            t += dtNextBrick + 1e-3;
            continue;
        }

        // Inside an active brick, take fixed steps
        float subT = 0.0;
        for (int j = 0; j < 32 && subT < dtNextBrick && t + subT < maxDist; j++) {
            float d = sample_voxel_pool(ro + rd * (t + subT), gPos, poolIdx, poolTex);
            accum_density += d * step_size;
            if (accum_density >= 1.0) {
                return vec4(1.0, t + subT, 0, 0);
            }
            subT += step_size;
        }
        t += dtNextBrick + 1e-3;
    }

    return vec4(accum_density, t, 0, 0);
}

#endif
