#ifndef TERRAIN_TEXTURES_GLSL
#define TERRAIN_TEXTURES_GLSL

uniform sampler2DArray uBiomeMap;
layout(binding = [[CHUNK_GRID_BINDING]]) uniform isampler2D u_chunkGrid;
uniform sampler2DArray uHeightmap;
layout(binding = [[HEIGHTMAP_ARRAY_BINDING]]) uniform sampler2DArray u_heightmapArray;
layout(binding = [[BIOME_MAP_BINDING]]) uniform sampler2DArray u_biomeMap;
layout(binding = [[MAX_HEIGHT_GRID_BINDING]]) uniform sampler2D u_maxHeightGrid;

#endif
