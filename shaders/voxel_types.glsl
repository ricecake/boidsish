#ifndef VOXEL_TYPES_GLSL
#define VOXEL_TYPES_GLSL

struct BrickMetadata {
    ivec4 gridPos_poolIdx; // xyz: gridPos, w: poolIndex
    vec4  timing_padding;  // x: lastUsedTime
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
