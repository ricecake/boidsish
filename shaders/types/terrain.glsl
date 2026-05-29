#ifndef TERRAIN_TYPES_GLSL
#define TERRAIN_TYPES_GLSL

struct AmbientProbe {
	vec4 sh_coeffs[9]; // rgb = coefficients, w = unused
};

#ifndef TERRAIN_DATA_BLOCK
#define TERRAIN_DATA_BLOCK
layout(std140, binding = [[TERRAIN_DATA_BINDING]]) uniform TerrainData {
	ivec4 u_originSize;    // x, z, size, is_bound
	vec4  u_terrainParams; // chunkSize, worldScale
	vec4  u_probeParams_unused;   // origin_x, origin_z, size_x, size_z
	vec4  u_probeParams2_unused;  // size_y, spacing, oct_res, origin_y
};
#endif

#ifndef TERRAIN_PROBES_BLOCK
#define TERRAIN_PROBES_BLOCK
layout(std430, binding = [[TERRAIN_PROBES_BINDING]]) buffer TerrainProbes {
	AmbientProbe u_terrainProbes[];
};
#endif

#endif
