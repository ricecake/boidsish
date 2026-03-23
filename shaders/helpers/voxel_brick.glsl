#ifndef VOXEL_BRICK_GLSL
#define VOXEL_BRICK_GLSL

// Global constants registered in C++
#define V_BRICK_SIZE [[VOXEL_BRICK_SIZE]]
#define V_VOXEL_SIZE [[VOXEL_SIZE]]
#define V_MAX_BRICKS [[VOXEL_MAX_BRICKS]]
#define V_HASH_SIZE  [[VOXEL_HASH_TABLE_SIZE]]

struct VoxelBrickMetadata {
    ivec4 grid_pos; // xyz: grid pos, w: pool index
};

layout(std430, binding = 35) buffer VoxelBrickMetadataBuffer {
    VoxelBrickMetadata brick_metadata[V_MAX_BRICKS];
};

layout(std430, binding = 38) buffer VoxelHashTableBuffer {
    int hash_table[V_HASH_SIZE];
};

layout(std430, binding = 40) buffer VoxelProposedGridPosBuffer {
    ivec4 proposed_grid_pos[V_HASH_SIZE];
};

layout(std430, binding = 36) buffer VoxelBrickVotesBuffer {
    int votes[V_HASH_SIZE];
};

layout(std430, binding = 39) buffer VoxelAccumulationBuffer {
    uint accumulation_data[]; // density, dirX, dirY, dirZ (each uint)
};

// --- Hashing and Morton Codes ---

uint morton_part(uint x) {
    x = (x | (x << 16)) & 0x030000FF;
    x = (x | (x << 8)) & 0x0300F00F;
    x = (x | (x << 4)) & 0x030C30C3;
    x = (x | (x << 2)) & 0x09249249;
    return x;
}

uint get_morton_code(uvec3 grid_pos) {
    return (morton_part(grid_pos.x) | (morton_part(grid_pos.y) << 1) | (morton_part(grid_pos.z) << 2));
}

uint hash_grid_pos(ivec3 grid_pos) {
    uint h = uint(grid_pos.x * 73856093) ^ uint(grid_pos.y * 19349663) ^ uint(grid_pos.z * 83492791);
    return h % uint(V_HASH_SIZE);
}

// --- Coordinate Mapping ---

ivec3 world_to_grid(vec3 world_pos) {
    float brick_world_size = float(V_BRICK_SIZE) * V_VOXEL_SIZE;
    return ivec3(floor(world_pos / brick_world_size));
}

ivec3 get_brick_origin_in_pool(int pool_index) {
    // pool_index to 3D pool coordinate
    int bricks_per_row = 16; // 16x16x16 bricks for 4096 capacity
    int bx = pool_index % bricks_per_row;
    int by = (pool_index / bricks_per_row) % bricks_per_row;
    int bz = pool_index / (bricks_per_row * bricks_per_row);
    return ivec3(bx, by, bz) * V_BRICK_SIZE;
}

// Find brick in pool for a given world position
int find_brick_in_pool(vec3 world_pos) {
    ivec3 grid_pos = world_to_grid(world_pos);
    uint h = hash_grid_pos(grid_pos);

    // Simple linear probing for hash collision
    for (int i = 0; i < 8; ++i) {
        int brick_idx = hash_table[(h + i) % uint(V_HASH_SIZE)];
        if (brick_idx == -1) break;
        if (brick_metadata[brick_idx].grid_pos.xyz == grid_pos) {
            return brick_idx;
        }
    }
    return -1;
}

// Map world position to pool texture coordinate
ivec3 world_to_pool_coords(vec3 world_pos, int brick_idx) {
    float brick_world_size = float(V_BRICK_SIZE) * V_VOXEL_SIZE;
    vec3 local_pos = mod(world_pos, brick_world_size);
    ivec3 voxel_pos = ivec3(floor(local_pos / V_VOXEL_SIZE));

    ivec3 pool_origin = get_brick_origin_in_pool(brick_idx);
    return pool_origin + voxel_pos;
}

#endif
