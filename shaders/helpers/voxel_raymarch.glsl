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

// Accurate raymarcher with brick skipping
// Returns accumulated density and total distance
vec4 raymarch_density(vec3 ro, vec3 rd, float maxDist, sampler3D poolTex) {
    float brick_world_size = float(VOXEL_BRICK_SIZE) * VOXEL_SIZE;
    float step_size = VOXEL_SIZE;

    float accum_density = 0.0;
    float t = 1e-3; // Start slightly away from origin to avoid self-intersection

    // Grid traversal with brick skipping
    for (int i = 0; i < 512 && t < maxDist; i++) {
        vec3 p = ro + rd * t;
        ivec3 gPos = world_to_grid(p);
        int poolIdx = lookup_brick(gPos);

        // Boundaries of current grid cell
        vec3 bMin = vec3(gPos) * brick_world_size;
        vec3 bMax = bMin + vec3(brick_world_size);

        // Distance to exit current brick along rd
        vec3 invRd = 1.0 / (rd + vec3(1e-9));
        vec3 tExit = (mix(bMin, bMax, step(0.0, rd)) - p) * invRd;
        float dtNextBrick = min(min(tExit.x, tExit.y), tExit.z);
        dtNextBrick = max(1e-4, dtNextBrick);

        if (poolIdx != -1) {
            // Inside an active brick: take sub-steps
            float subT = 0.0;
            for (int j = 0; j < 32 && subT < dtNextBrick && (t + subT) < maxDist; j++) {
                float d = sample_voxel_pool(p + rd * subT, gPos, poolIdx, poolTex);
                accum_density += d * step_size;
                if (accum_density >= 1.0) {
                    return vec4(1.0, t + subT, 0, 0);
                }
                subT += step_size;
            }
        }

        // Move to the next brick
        t += dtNextBrick + 1e-4;
    }

    return vec4(accum_density, t, 0, 0);
}

#endif
