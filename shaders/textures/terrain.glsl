#ifndef TERRAIN_TEXTURES_GLSL
#define TERRAIN_TEXTURES_GLSL

#ifndef BIOME_MAP_DEFINED
#define BIOME_MAP_DEFINED
layout(binding = 14) uniform sampler2DArray uBiomeMap; // Legacy, kept for compat
#endif

#ifndef TERRAIN_GRID_DEFINED
#define TERRAIN_GRID_DEFINED
layout(binding = [[TERRAIN_GRID_BINDING]]) uniform isampler2D u_chunkGrid;
#endif

#ifndef HEIGHTMAP_DEFINED
#define HEIGHTMAP_DEFINED
uniform sampler2DArray uHeightmap; // Legacy, kept for compat
#endif

#ifndef HEIGHTMAP_ARRAY_DEFINED
#define HEIGHTMAP_ARRAY_DEFINED
layout(binding = [[BAKED_HEIGHTMAP_BINDING]]) uniform sampler2DArray u_heightmapArray;
#endif

#ifndef TERRAIN_BIOME_MAP_DEFINED
#define TERRAIN_BIOME_MAP_DEFINED
layout(binding = 14) uniform sampler2DArray u_biomeMap; // Sync with BIOME_MAP_IMAGE_BINDING later
#endif

#ifndef MAX_HEIGHT_GRID_DEFINED
#define MAX_HEIGHT_GRID_DEFINED
layout(binding = [[MAX_HEIGHT_GRID_BINDING]]) uniform sampler2D u_maxHeightGrid;
#endif

#endif
