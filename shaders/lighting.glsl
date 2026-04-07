#ifndef LIGHTING_GLSL
#define LIGHTING_GLSL

struct Light {
	vec3  position;
	float intensity;
	vec3  color;
	int   type;
	vec3  direction;
	float inner_cutoff; // Also: emissive_radius (EMISSIVE), flash_radius (FLASH)
	float outer_cutoff; // Also: falloff_exp (FLASH)
};

struct AmbientProbe {
	vec4 sh_coeffs[9]; // rgb = coefficients, w = unused
};

const int MAX_LIGHTS = [[MAX_LIGHTS]];
const int MAX_SHADOW_MAPS = [[MAX_SHADOW_MAPS]];
const int MAX_CASCADES = [[MAX_CASCADES]];

layout(std140) uniform Lighting {
	Light lights[MAX_LIGHTS];
	int   num_lights;
	float worldScale;
	float dayTime;
	float nightFactor;
	vec3  viewPos;
	float cloudShadowIntensity;
	vec3  ambient_light;
	float time;
	vec3  viewDir;
	float cloudAltitude;
	float cloudThickness;
	float cloudDensity;
	float cloudCoverage;
	float cloudWarp;
	float cloudPhaseG1;
	float cloudPhaseG2;
	float cloudPhaseAlpha;
	float cloudPhaseIsotropic;
	float cloudPowderScale;
	float cloudPowderMultiplier;
	float cloudPowderLocalScale;
	float cloudShadowOpticalDepthMultiplier;
	float cloudShadowStepMultiplier;
	float cloudSunLightScale;
	float cloudMoonLightScale;
	float cloudBeerPowderMix;
	vec4  sh_coeffs[9];
};

#ifndef TERRAIN_DATA_BLOCK
#define TERRAIN_DATA_BLOCK
layout(std140, binding = [[TERRAIN_DATA_BINDING]]) uniform TerrainData {
	ivec4 u_originSize;    // x, z, size, is_bound
	vec4  u_terrainParams; // chunkSize, worldScale
};
#endif

#ifndef TERRAIN_PROBES_BLOCK
#define TERRAIN_PROBES_BLOCK
layout(std430, binding = [[TERRAIN_PROBES_BINDING]]) buffer TerrainProbes {
	AmbientProbe u_terrainProbes[];
};
#endif

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

// Shadow mapping UBO (binding set via glUniformBlockBinding to point 2)
layout(std140, binding = 2) uniform Shadows {
	mat4 lightSpaceMatrices[MAX_SHADOW_MAPS];
	vec4 cascadeSplits;
	int  numShadowLights;
};

// Shadow map texture array - bound to texture unit 4
uniform sampler2DArrayShadow shadowMaps;

// Per-light shadow map index (-1 if no shadow for this light)
// This is set via uniform since the Light struct can't easily store it
uniform int lightShadowIndices[MAX_LIGHTS];

#endif
