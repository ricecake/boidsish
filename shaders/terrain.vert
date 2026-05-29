#version 460 core

// Per-vertex attributes (from flat grid mesh)
layout(location = 0) in vec3 aPos;       // Flat grid position (x, 0, z) in [0, chunk_size]
layout(location = 1) in vec2 aTexCoords; // Heightmap UV [0, 1]

#include "helpers/constants.glsl"
#include "helpers/lighting.glsl"

struct PatchDrawData {
	vec4 world_offset_and_slice; // xyz = world offset, w = texture slice index
	vec4 patch_coords_and_size;  // xy = patch local coords (0..PatchesPerChunkSide-1), z = patch size, w = chunk size
};

layout(std430, binding = [[TERRAIN_PATCH_DRAW_DATA_BINDING]]) readonly buffer TerrainPatchDrawData {
	PatchDrawData patchDrawData[];
};

out vec3       LocalPos_VS_out;  // Local grid position
out vec2       TexCoords_VS_out; // Heightmap UV
out vec3       viewForward;
flat out float TextureSlice_VS_out;
flat out vec3  WorldOffset_VS_out;
flat out vec4  PatchInfo_VS_out;    // xy = patch local coords, z = patch size, w = chunk size
flat out int   DrawID_VS_out;

void main() {
	// Extract camera forward vector
	viewForward = vec3(-view[0][2], -view[1][2], -view[2][2]);

	int drawID = gl_InstanceID;
	PatchDrawData draw = patchDrawData[drawID];

	// aPos is in [0, patch_size]
	// localPatchOffset = patchCoords * patch_size
	float patchSize = draw.patch_coords_and_size.z;
	vec2 localPatchOffset = draw.patch_coords_and_size.xy * patchSize;

	// Local position within the chunk [0, chunk_size]
	vec3 localChunkPos = aPos + vec3(localPatchOffset.x, 0.0, localPatchOffset.y);

	LocalPos_VS_out = localChunkPos;

	// Heightmap UV
	float chunkSize = draw.patch_coords_and_size.w;
	TexCoords_VS_out = localChunkPos.xz / chunkSize;

	TextureSlice_VS_out = draw.world_offset_and_slice.w;
	WorldOffset_VS_out = draw.world_offset_and_slice.xyz;
	PatchInfo_VS_out = draw.patch_coords_and_size;
	DrawID_VS_out = drawID;

	gl_Position = vec4(localChunkPos, 1.0);
}
