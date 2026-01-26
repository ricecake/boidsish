#version 420 core

// Per-vertex attributes (from flat grid mesh)
layout(location = 0) in vec3 aPos;           // Flat grid position (x, 0, z) in [0, chunk_size]
layout(location = 1) in vec2 aTexCoords;     // Heightmap UV [0, 1]

// Per-instance attributes (from instance buffer)
layout(location = 3) in vec4 aWorldOffsetAndSlice;  // xyz = world offset, w = texture slice index
layout(location = 4) in vec4 aBounds;               // x = min_y, y = max_y (for debugging/culling)

out vec3 LocalPos_VS_out;      // Local grid position (for TCS to transform)
out vec2 TexCoords_VS_out;     // Heightmap UV
out vec3 viewForward;
flat out float TextureSlice_VS_out;  // Which slice in texture array
flat out vec3 WorldOffset_VS_out;    // World offset for this chunk

uniform mat4 view;

void main() {
    // Extract camera forward vector
    viewForward = vec3(-view[0][2], -view[1][2], -view[2][2]);

    // Pass through local position and UVs
    LocalPos_VS_out = aPos;
    TexCoords_VS_out = aTexCoords;

    // Pass instance data to later stages
    TextureSlice_VS_out = aWorldOffsetAndSlice.w;
    WorldOffset_VS_out = aWorldOffsetAndSlice.xyz;

    // Output flat local position (Y will be displaced in TES after heightmap lookup)
    gl_Position = vec4(aPos, 1.0);
}
