#version 420 core

// Per-vertex attributes (from small patch quad mesh)
layout(location = 0) in vec3 aPos;       // Range [0, 1]
layout(location = 1) in vec2 aTexCoords; // Range [0, 1]

struct ChunkMetadata {
	vec4  world_offset_slice; // x, z, slice, active
	vec4  bounds;             // min_y, max_y, 0, 0
};

struct VisiblePatch {
	uint chunk_index;
	uint patch_x; // 0-7
	uint patch_z; // 0-7
	uint _padding;
};

layout(std430, binding = 0) readonly buffer ChunkMetadataBuffer {
	ChunkMetadata chunks[];
};

layout(std430, binding = 29) readonly buffer VisiblePatchBuffer {
	VisiblePatch visible_patches[];
};

out vec3       LocalPos_VS_out;  // Local grid position
out vec2       TexCoords_VS_out; // Heightmap UV
out vec3       viewForward;
flat out float TextureSlice_VS_out; // Which slice in texture array
flat out vec3  WorldOffset_VS_out;  // World offset for this chunk
flat out vec4  Bounds_VS_out;       // Min/max Y bounds

uniform mat4  view;
uniform float uChunkSize;

void main() {
	// Extract camera forward vector
	viewForward = vec3(-view[0][2], -view[1][2], -view[2][2]);

	VisiblePatch patch = visible_patches[gl_InstanceID];
	ChunkMetadata chunk = chunks[patch.chunk_index];

	float patchSize = uChunkSize / 8.0;
	vec2  patchOffset = vec2(float(patch.patch_x), float(patch.patch_z)) * patchSize;

	// Scale and offset local position
	vec3 localPos = aPos;
	localPos.xz *= patchSize;
	localPos.xz += patchOffset;

	// Calculate UV for the whole chunk
	vec2 uv = localPos.xz / uChunkSize;

	LocalPos_VS_out = localPos;
	TexCoords_VS_out = uv;

	TextureSlice_VS_out = chunk.world_offset_slice.z;
	WorldOffset_VS_out = vec3(chunk.world_offset_slice.x, 0.0, chunk.world_offset_slice.y);
	Bounds_VS_out = chunk.bounds;

	gl_Position = vec4(localPos, 1.0);
}
