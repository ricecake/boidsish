#ifndef TERRAIN_COMMON_STRUCTS_GLSL
#define TERRAIN_COMMON_STRUCTS_GLSL

struct TerrainDataUbo {
	ivec4 origin_size;
	vec4  terrain_params; // x,y,z = focusPos, w = worldScale
};

layout(std140, binding = 8) uniform TerrainData {
    TerrainDataUbo u_terrain;
};

#define worldScale u_terrain.terrain_params.y

#endif // TERRAIN_COMMON_STRUCTS_GLSL
