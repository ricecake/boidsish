#ifndef VOXEL_RAYMARCH_GLSL
#define VOXEL_RAYMARCH_GLSL

#include "voxel_brick.glsl"

uniform sampler3D u_brickPool; // Texture unit 19 (VOXEL_BRICK_POOL_UNIT)

struct Ray {
    vec3 origin;
    vec3 direction;
};

struct RaymarchResult {
    vec3  pos;
    float dist;
    float density;
    vec3  direction;
};

// --- DDA-based Raymarching through Sparsely Populated Bricks ---

RaymarchResult march_voxels(Ray ray, float max_dist, float step_size) {
    RaymarchResult res;
    res.pos = ray.origin;
    res.dist = 0.0;
    res.density = 0.0;
    res.direction = vec3(0.0);

    float brick_world_size = float(V_BRICK_SIZE) * V_VOXEL_SIZE;

    // Ray start and direction setup
    vec3 p = ray.origin;
    vec3 d = normalize(ray.direction + vec3(1e-6)); // Add epsilon to avoid div-by-zero
    float t = 0.0;

    while (t < max_dist) {
        ivec3 grid_pos = world_to_grid(p);
        int brick_idx = find_brick_in_pool(p);

        if (brick_idx != -1) {
            // Found a brick! March through it at high resolution
            ivec3 pool_origin = get_brick_origin_in_pool(brick_idx);

            // March until we exit the brick
            float brick_exit_t = t + brick_world_size;
            while (t < brick_exit_t && t < max_dist) {
                vec3 pool_uv = (vec3(pool_origin) + mod(p, brick_world_size) / V_VOXEL_SIZE) / (16.0 * float(V_BRICK_SIZE));
                vec4 sample_data = texture(u_brickPool, pool_uv);

                if (sample_data.a > 0.01) {
                    res.pos = p;
                    res.dist = t;
                    res.density = sample_data.a;
                    res.direction = sample_data.rgb;
                    return res;
                }

                t += step_size;
                p = ray.origin + t * d;

                // Check if we exited current brick
                if (any(notEqual(world_to_grid(p), grid_pos))) break;
            }
        } else {
            // Empty space - skip to the next brick boundary using DDA logic
            vec3  grid_step = sign(d);
            vec3  cell_boundary = (vec3(grid_pos) + max(vec3(0.0), grid_step)) * brick_world_size;
            vec3  t_max = (cell_boundary - ray.origin) / d;

            float skip_t = min(min(t_max.x, t_max.y), t_max.z);
            t = skip_t + 0.001; // Tiny offset to enter next brick
            p = ray.origin + t * d;
        }
    }

    return res;
}

#endif
