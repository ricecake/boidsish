#ifndef VOXEL_RAYMARCH_GLSL
#define VOXEL_RAYMARCH_GLSL

#include "voxel_brick.glsl"

float sample_voxel_density(vec3 worldPos, sampler3D poolTex) {
    ivec3 gPos = world_to_grid(worldPos);
    int poolIdx = lookup_brick(gPos);
    if (poolIdx == -1) return 0.0;

    float brick_world_size = float(VOXEL_BRICK_SIZE) * VOXEL_SIZE;
    vec3 localPos = (worldPos - (vec3(gPos) * brick_world_size)) / brick_world_size;

    // Clamp localPos to [0, 1] for safety
    localPos = clamp(localPos, 0.0, 1.0);

    vec3 uv = pool_index_to_uv(poolIdx, localPos);
    return texture(poolTex, uv).r;
}

// Simple raymarcher that skips empty bricks
// Returns accumulated density and total distance
vec4 raymarch_density(vec3 ro, vec3 rd, float maxDist, sampler3D poolTex) {
    float brick_world_size = float(VOXEL_BRICK_SIZE) * VOXEL_SIZE;
    float step_size = VOXEL_SIZE * 0.5; // Half voxel step for quality

    float accum_density = 0.0;
    float t = 0.0;

    // We could use DDA to skip empty bricks, but for now let's just do fixed steps
    // with a per-brick check for performance.

    while (t < maxDist) {
        vec3 p = ro + rd * t;
        ivec3 gPos = world_to_grid(p);

        int poolIdx = lookup_brick(gPos);
        if (poolIdx == -1) {
            // SKIP BRICK
            // Calculate distance to leave this brick along rd
            vec3 brickCorner = vec3(gPos) * brick_world_size;
            vec3 distToExit = (brickCorner + brick_world_size * (step(0.0, rd)) - p) / rd;
            float skipT = min(min(distToExit.x, distToExit.y), distToExit.z);
            t += max(skipT + step_size, step_size);
            continue;
        }

        // Inside an active brick, take steps
        // ... (can also use DDA here or just fixed steps)
        float d = sample_voxel_density(p, poolTex);
        accum_density += d * step_size;

        if (accum_density >= 1.0) break;

        t += step_size;
    }

    return vec4(accum_density, t, 0, 0);
}

#endif
