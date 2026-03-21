#ifndef VOXEL_BRICK_GLSL
#define VOXEL_BRICK_GLSL

#include "../voxel_types.glsl"

// Use constants registered in C++
#ifndef VOXEL_BRICK_SIZE
#define VOXEL_BRICK_SIZE 8
#endif

#ifndef VOXEL_SIZE
#define VOXEL_SIZE 0.25
#endif

#ifndef VOXEL_HASH_TABLE_SIZE
#define VOXEL_HASH_TABLE_SIZE 16384
#endif

#ifndef VOXEL_BRICKS_PER_DIM
#define VOXEL_BRICKS_PER_DIM 16
#endif

uint get_brick_hash(ivec3 gridPos) {
    // Spatial hash using large primes (similar to spatial_hash.glsl)
	uint h = uint(gridPos.x * 73856093) ^ uint(gridPos.y * 19349663) ^ uint(gridPos.z * 83492791);
	return h % VOXEL_HASH_TABLE_SIZE;
}

ivec3 world_to_grid(vec3 worldPos) {
    return ivec3(floor(worldPos / (float(VOXEL_BRICK_SIZE) * VOXEL_SIZE)));
}

// poolIdx to 3D texture UV
vec3 pool_index_to_uv(int poolIdx, vec3 localPos) {
    // poolIdx -> (px, py, pz) where tex_dim / brick_size = bricks_per_dim
    int px = poolIdx % VOXEL_BRICKS_PER_DIM;
    int py = (poolIdx / VOXEL_BRICKS_PER_DIM) % VOXEL_BRICKS_PER_DIM;
    int pz = (poolIdx / (VOXEL_BRICKS_PER_DIM * VOXEL_BRICKS_PER_DIM)) % VOXEL_BRICKS_PER_DIM;

    float tex_dim = float(VOXEL_BRICKS_PER_DIM * VOXEL_BRICK_SIZE);

    // localPos is [0, 1] within the brick
    vec3 brickCorner = vec3(px, py, pz) * float(VOXEL_BRICK_SIZE);
    vec3 texPos = brickCorner + (localPos * float(VOXEL_BRICK_SIZE));

    return texPos / tex_dim;
}

ivec3 pool_index_to_coord(int poolIdx, vec3 localPos) {
    int px = poolIdx % VOXEL_BRICKS_PER_DIM;
    int py = (poolIdx / VOXEL_BRICKS_PER_DIM) % VOXEL_BRICKS_PER_DIM;
    int pz = (poolIdx / (VOXEL_BRICKS_PER_DIM * VOXEL_BRICKS_PER_DIM)) % VOXEL_BRICKS_PER_DIM;

    ivec3 brickCorner = ivec3(px, py, pz) * VOXEL_BRICK_SIZE;
    ivec3 localCoord = ivec3(clamp(floor(localPos * float(VOXEL_BRICK_SIZE)), vec3(0.0), vec3(float(VOXEL_BRICK_SIZE) - 1.0)));
    return brickCorner + localCoord;
}

int lookup_brick(ivec3 gridPos) {
    uint slot = get_brick_hash(gridPos);
    if (brick_metadata[slot].poolIndex != -1 && brick_metadata[slot].gridPos == gridPos) {
        return brick_metadata[slot].poolIndex;
    }
    return -1;
}

#endif
