#ifndef VOXEL_TYPES_GLSL
#define VOXEL_TYPES_GLSL

struct BrickMetadata {
    ivec3 gridPos;      // 12 bytes
    int   poolIndex;    // 4 bytes -> total 16
    float lastUsedTime; // 4 bytes
    int   padding[3];   // 12 bytes -> total 32 bytes
};

layout(std430, binding = 36) buffer VoxelBrickVotes {
    uint brick_votes[]; // size HashTableSize
};

layout(std430, binding = 37) buffer VoxelFreeList {
    int free_count;     // atomic counter
    int free_indices[]; // size MaxBricks
};

layout(std430, binding = 38) buffer VoxelBrickMetadata {
    BrickMetadata brick_metadata[]; // size HashTableSize
};

#endif
