#ifndef BIOME_TYPES_GLSL
#define BIOME_TYPES_GLSL

struct BiomeShaderProperties {
	vec4 albedo_roughness; // rgb = albedo, w = roughness
	vec4 params;           // x = metallic, y = detailStrength, z = detailScale, w = noiseType
};

#ifndef BIOME_DATA_BLOCK
#define BIOME_DATA_BLOCK
layout(std140, binding = [[BIOME_DATA_BINDING]]) uniform BiomeData {
	BiomeShaderProperties u_biomes[8];
};
#endif

#endif
