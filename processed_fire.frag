#version 460 core
#ifndef GSHADERS_FIRE_FRAG
#define GSHADERS_FIRE_FRAG

//START shaders/lighting.glsl
#ifndef GSHADERS_LIGHTING_GLSL
#define GSHADERS_LIGHTING_GLSL
#ifndef LIGHTING_GLSL
#define LIGHTING_GLSL

//START shaders/types/lighting.glsl
#ifndef GSHADERS_TYPES_LIGHTING_GLSL
#define GSHADERS_TYPES_LIGHTING_GLSL
#ifndef LIGHTING_TYPES_GLSL
#define LIGHTING_TYPES_GLSL

struct Light {
	vec3  position;
	float intensity;
	vec3  color;
	int   type;
	vec3  direction;
	float inner_cutoff; // Also: emissive_radius (EMISSIVE), flash_radius (FLASH)
	float outer_cutoff; // Also: falloff_exp (FLASH)
};

const int MAX_LIGHTS = 10;

layout(std140, binding = 0) uniform Lighting {
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
	mat4  cloudShadowMatrix;
	vec4  sh_coeffs[9];
};

#endif
#endif // GSHADERS_TYPES_LIGHTING_GLSL
//END shaders/types/lighting.glsl (returning to shaders/lighting.glsl)
//START shaders/types/terrain.glsl
#ifndef GSHADERS_TYPES_TERRAIN_GLSL
#define GSHADERS_TYPES_TERRAIN_GLSL
#ifndef TERRAIN_TYPES_GLSL
#define TERRAIN_TYPES_GLSL

struct AmbientProbe {
	vec4 sh_coeffs[9]; // rgb = coefficients, w = unused
};

#ifndef TERRAIN_DATA_BLOCK
#define TERRAIN_DATA_BLOCK
layout(std140, binding = 8) uniform TerrainData {
	ivec4 u_originSize;    // x, z, size, is_bound
	vec4  u_terrainParams; // chunkSize, worldScale
};
#endif

#ifndef TERRAIN_PROBES_BLOCK
#define TERRAIN_PROBES_BLOCK
layout(std430, binding = 29) buffer TerrainProbes {
	AmbientProbe u_terrainProbes[];
};
#endif

#endif
#endif // GSHADERS_TYPES_TERRAIN_GLSL
//END shaders/types/terrain.glsl (returning to shaders/lighting.glsl)
//START shaders/types/biomes.glsl
#ifndef GSHADERS_TYPES_BIOMES_GLSL
#define GSHADERS_TYPES_BIOMES_GLSL
#ifndef BIOME_TYPES_GLSL
#define BIOME_TYPES_GLSL

struct BiomeShaderProperties {
	vec4 albedo_roughness; // rgb = albedo, w = roughness
	vec4 params;           // x = metallic, y = detailStrength, z = detailScale, w = noiseType
};

#ifndef BIOME_DATA_BLOCK
#define BIOME_DATA_BLOCK
layout(std140, binding = 7) uniform BiomeData {
	BiomeShaderProperties u_biomes[8];
};
#endif

#endif
#endif // GSHADERS_TYPES_BIOMES_GLSL
//END shaders/types/biomes.glsl (returning to shaders/lighting.glsl)
//START shaders/types/shadows.glsl
#ifndef GSHADERS_TYPES_SHADOWS_GLSL
#define GSHADERS_TYPES_SHADOWS_GLSL
#ifndef SHADOW_TYPES_GLSL
#define SHADOW_TYPES_GLSL

const int MAX_SHADOW_MAPS = 16;
const int MAX_CASCADES = 4;

// Shadow mapping UBO (binding set via glUniformBlockBinding to point 2)
layout(std140, binding = 2) uniform Shadows {
	mat4 lightSpaceMatrices[MAX_SHADOW_MAPS];
	vec4 cascadeSplits;
	int  numShadowLights;
};

// Per-light shadow map index (-1 if no shadow for this light)
// This is set via uniform since the Light struct can't easily store it
uniform int lightShadowIndices[10];

#endif
#endif // GSHADERS_TYPES_SHADOWS_GLSL
//END shaders/types/shadows.glsl (returning to shaders/lighting.glsl)
//START shaders/textures/shadows.glsl
#ifndef GSHADERS_TEXTURES_SHADOWS_GLSL
#define GSHADERS_TEXTURES_SHADOWS_GLSL
#ifndef SHADOW_TEXTURES_GLSL
#define SHADOW_TEXTURES_GLSL

//START shaders/types/shadows.glsl
//END shaders/types/shadows.glsl (returning to shaders/textures/shadows.glsl)

// Shadow map texture array - bound to texture unit 4
uniform sampler2DArrayShadow shadowMaps;

#endif
#endif // GSHADERS_TEXTURES_SHADOWS_GLSL
//END shaders/textures/shadows.glsl (returning to shaders/lighting.glsl)

#endif
#endif // GSHADERS_LIGHTING_GLSL
//END shaders/lighting.glsl (returning to shaders/fire.frag)
//START shaders/particle_types.glsl
#ifndef GSHADERS_PARTICLE_TYPES_GLSL
#define GSHADERS_PARTICLE_TYPES_GLSL
//START shaders/types/particle_types.glsl
#ifndef GSHADERS_TYPES_PARTICLE_TYPES_GLSL
#define GSHADERS_TYPES_PARTICLE_TYPES_GLSL
#ifndef PARTICLE_TYPES_GLSL
#define PARTICLE_TYPES_GLSL

#define STYLE_ROCKET_TRAIL 0
#define STYLE_EXPLOSION 1
#define STYLE_FIRE 2
#define STYLE_SPARKS 3
#define STYLE_GLITTER 4
#define STYLE_AMBIENT 5
#define STYLE_BUBBLES 6
#define STYLE_FIREFLIES 7
#define STYLE_DEBUG 8
#define STYLE_CINDER 9
#define STYLE_RAIN 10
#define STYLE_SNOW 11
#define STYLE_LEAF 12
#define STYLE_PETAL 13
#define STYLE_BIRDS 14
#define STYLE_FAIRY 16
#define STYLE_IRIDESCENT 28

// Must match the C++ struct layout in fire_effect_manager.cpp
struct Particle {
	vec4  pos;           // x, y, z, lifetime
	vec4  vel;           // x, y, z, size
	vec4  color;         // r, g, b, a
	vec4  origin;        // x, y, z, intensity
	float phase;
	float counter;
	int   style;
	int   emitter_id;
	int   emitter_index;
	int   _padding[3];
};

// Must match the C++ Emitter struct in fire_effect_manager.h
struct Emitter {
	vec3  position;   // 12 bytes
	int   style;      // 4 bytes -> total 16
	vec3  direction;  // 12 bytes
	int   is_active;  // 4 bytes -> total 16
	vec3  velocity;   // 12 bytes
	int   id;         // 4 bytes -> total 16
	vec3  dimensions; // 12 bytes
	int   type;       // 4 bytes -> total 16
	float sweep;      // 4 bytes
	int   use_slice_data;
	int   slice_data_offset;
	int   slice_data_count;
	float slice_area;
	int   request_clear;
	int   _padding_emitter[2];
};

struct ChunkInfo {
	vec2  worldOffset;
	float slice;
	float size;
};

struct DrawArraysIndirectCommand {
	uint count;
	uint instanceCount;
	uint first;
	uint baseInstance;
};

struct DispatchIndirectCommand {
	uint num_groups_x;
	uint num_groups_y;
	uint num_groups_z;
	uint count;
};

struct ParticleStats {
	uint count_birds;
	uint count_leaves;
	uint count_petals;
	uint count_bubbles;
	uint count_fireflies;
	uint count_snow;
	uint count_fairies;
	uint _unused_counts[1];

	uint limit_birds;
	uint limit_leaves;
	uint limit_petals;
	uint limit_bubbles;
	uint limit_fireflies;
	uint limit_snow;
	uint limit_fairies;
	uint _unused_limits[1];
};

#ifdef COMPUTE_SHADER
layout(std430, binding = 16) buffer ParticleBuffer {
	Particle particles[];
};

layout(std430, binding = 22) buffer EmitterBuffer {
	Emitter emitters[];
};

layout(std430, binding = 27) writeonly buffer VisibleIndicesBuffer {
	uint visible_indices[];
};

layout(std430, binding = 33) buffer LiveIndicesBuffer {
	uint live_indices[];
};

layout(std430, binding = 14) buffer ParticleGridHeads {
	int grid_heads[];
};

layout(std430, binding = 15) buffer ParticleGridNext {
	int grid_next[];
};
#else
layout(std430, binding = 16) readonly buffer ParticleBuffer {
	Particle particles[];
};

layout(std430, binding = 22) readonly buffer EmitterBuffer {
	Emitter emitters[];
};

layout(std430, binding = 27) readonly buffer VisibleIndicesBuffer {
	uint visible_indices[];
};

layout(std430, binding = 33) readonly buffer LiveIndicesBuffer {
	uint live_indices[];
};

layout(std430, binding = 14) readonly buffer ParticleGridHeads {
	int grid_heads[];
};

layout(std430, binding = 15) readonly buffer ParticleGridNext {
	int grid_next[];
};
#endif

layout(std430, binding = 17) buffer IndirectionBuffer {
	int particle_to_emitter_map[];
};

layout(std430, binding = 23) buffer ChunkInfoBuffer {
	ChunkInfo chunks[];
};

layout(std430, binding = 24) buffer SliceDataBuffer {
	vec4 slice_data[];
};

layout(std430, binding = 28) buffer DrawCommandBuffer {
	DrawArraysIndirectCommand draw_command;
};

layout(std430, binding = 34) buffer BehaviorCommandBuffer {
	DispatchIndirectCommand behavior_command;
};

layout(std430, binding = 52) buffer ParticleStatsBuffer {
	ParticleStats stats;
};

#endif
#endif // GSHADERS_TYPES_PARTICLE_TYPES_GLSL
//END shaders/types/particle_types.glsl (returning to shaders/particle_types.glsl)
#endif // GSHADERS_PARTICLE_TYPES_GLSL
//END shaders/particle_types.glsl (returning to shaders/fire.frag)
//START shaders/atmosphere/common.glsl
#ifndef GSHADERS_ATMOSPHERE_COMMON_GLSL
#define GSHADERS_ATMOSPHERE_COMMON_GLSL
#ifndef ATMOSPHERE_COMMON_GLSL
#define ATMOSPHERE_COMMON_GLSL

//START shaders/atmosphere/../helpers/constants.glsl
#ifndef GSHADERS_HELPERS_CONSTANTS_GLSL
#define GSHADERS_HELPERS_CONSTANTS_GLSL
#ifndef CONSTANTS
#define CONSTANTS

const float PI = 3.14159265359;

#endif
#endif // GSHADERS_HELPERS_CONSTANTS_GLSL
//END shaders/atmosphere/../helpers/constants.glsl (returning to shaders/atmosphere/common.glsl)

// Physical Constants
const float kEarthRadius = 6360.0; // km

#ifndef ATMOSPHERE_HEIGHT_DEFINED
	#define ATMOSPHERE_HEIGHT_DEFINED
uniform float u_atmosphereHeight;
#endif

#ifndef TRANSMITTANCE_LUT_DEFINED
	#define TRANSMITTANCE_LUT_DEFINED
uniform sampler2D u_transmittanceLUT;
#endif

#ifndef WEATHER_TEXTURES_DEFINED
	#define WEATHER_TEXTURES_DEFINED
layout(binding = 42) uniform sampler2D u_weatherScalars;
layout(binding = 43) uniform sampler2D u_weatherAerosols;
#endif

#ifndef TERRAIN_DATA_BLOCK
#define TERRAIN_DATA_BLOCK
layout(std140, binding = 8) uniform TerrainData {
	ivec4 u_originSize;    // x, z, size, is_bound
	vec4  u_terrainParams; // chunkSize, worldScale
};
#endif

#ifndef WORLD_SCALE_VALUE
	#define WORLD_SCALE_VALUE u_terrainParams.y
#endif

#define kAtmosphereHeight u_atmosphereHeight
#define kTopRadius (kEarthRadius + kAtmosphereHeight)

uniform vec3  u_rayleighScatteringBase;
uniform float u_rayleighScaleHeight;
#define kRayleighScattering u_rayleighScatteringBase
#define kRayleighScaleHeight u_rayleighScaleHeight

uniform float u_mieScatteringBase;
uniform float u_mieExtinctionBase;
uniform float u_mieScaleHeight;
#define kMieScattering u_mieScatteringBase
#define kMieExtinction u_mieExtinctionBase
#define kMieScaleHeight u_mieScaleHeight

uniform float u_rayleighScale;
uniform float u_mieScale;
uniform float u_mieAnisotropy;

uniform vec3 u_ozoneAbsorptionBase;
#define kOzoneAbsorption u_ozoneAbsorptionBase

// Helper functions
bool intersectSphere(vec3 ro, vec3 rd, float radius, out float t0, out float t1) {
	float b = dot(ro, rd);
	float c = dot(ro, ro) - radius * radius;
	float det = b * b - c;
	if (det < 0.0)
		return false;
	det = sqrt(det);
	t0 = -b - det;
	t1 = -b + det;
	return true;
}

float getRayleighDensity(float h) {
	return exp(-max(0.0, h) / kRayleighScaleHeight);
}

float getMieDensity(float h) {
	return exp(-max(0.0, h) / kMieScaleHeight);
}

float getOzoneDensity(float h) {
	return max(0.0, 1.0 - abs(max(0.0, h) - 25.0) / 15.0);
}

struct Sampling {
	vec3 rayleigh;
	vec3 mie;
	vec3 extinction;
};

Sampling getAtmosphereProperties(float h) {
	float rd = getRayleighDensity(h);
	float md = getMieDensity(h);
	float od = getOzoneDensity(h);

	Sampling s;
	s.rayleigh = kRayleighScattering * rd * u_rayleighScale;
	s.mie = vec3(kMieScattering * md * u_mieScale);
	s.extinction = s.rayleigh + vec3(kMieExtinction * md * u_mieScale) + kOzoneAbsorption * od;
	return s;
}

Sampling getAtmospherePropertiesAtPos(vec3 worldPos) {
	float h = worldPos.y / (1000.0 * max(0.0001, WORLD_SCALE_VALUE));
	Sampling s = getAtmosphereProperties(h);

	// Modulate Mie based on weather
	// LBM grid is 128x128, each cell is 32.0 units (one chunk size)
	// u_originSize contains the anchor coordinates in chunk-space
	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;
	vec2 weatherUV = (worldPos.xz / scaledChunkSize - vec2(u_originSize.xy)) / 128.0;
	vec4 scalars = texture(u_weatherScalars, weatherUV);
	vec4 aerosols = texture(u_weatherAerosols, weatherUV);

	float humidity = scalars.y;
	float aerosolConc = aerosols.x + aerosols.y + aerosols.z + aerosols.w;

	// Humidity increases Mie scattering (haze/mist)
	float humidityFactor = 1.0 + humidity * 5.0;
	float aerosolFactor = 1.0 + aerosolConc * 10.0;

	s.mie *= humidityFactor * aerosolFactor;
	s.extinction += (s.mie - vec3(kMieScattering * getMieDensity(h) * u_mieScale)); // Re-calculate extinction diff

	return s;
}

// Phase Functions
float rayleighPhase(float cosTheta) {
	return 3.0 / (16.0 * PI) * (1.0 + cosTheta * cosTheta);
}

float miePhase(float cosTheta) {
	float g = u_mieAnisotropy;
	float g2 = g * g;
	return (1.0 - g2) / (4.0 * PI * pow(max(1e-4, 1.0 + g2 - 2.0 * g * cosTheta), 1.5));
}

// LUT mapping functions - Simple Linear mapping for Transmittance to avoid precision issues
vec2 transmittanceToUV(float r, float mu) {
	float x_mu = mu * 0.5 + 0.5;
	float x_r = (r - kEarthRadius) / kAtmosphereHeight;
	return vec2(x_mu, x_r);
}

void UVToTransmittance(vec2 uv, out float r, out float mu) {
	mu = uv.x * 2.0 - 1.0;
	r = kEarthRadius + uv.y * kAtmosphereHeight;
}

#endif
#endif // GSHADERS_ATMOSPHERE_COMMON_GLSL
//END shaders/atmosphere/common.glsl (returning to shaders/fire.frag)

in float         v_lifetime;
in vec4          view_pos;
in vec4          v_pos;
in vec3          v_vel;
in vec3          v_vel_view;
in vec3          v_origin;
flat in int      v_style;
flat in int      v_emitter_index;
flat in int      v_emitter_id;
flat in uint     v_particle_idx;
out vec4         FragColor;
flat in Particle v_p;

uniform float u_time;
uniform vec3  u_biomeAlbedos[8];
//START shaders/helpers/fast_noise.glsl
#ifndef GSHADERS_HELPERS_FAST_NOISE_GLSL
#define GSHADERS_HELPERS_FAST_NOISE_GLSL
#ifndef HELPERS_FAST_NOISE_GLSL
#define HELPERS_FAST_NOISE_GLSL

// Helper functions for fast texture-based noise lookups
// Requires noise texture samplers bound to fixed units:
// u_noiseTexture: 3D, unit 5, R=Simplex/G=Worley/B=FBM/A=Warped
// u_curlTexture: 3D, unit 6, RGB=Curl/A=FBM Curl Mag
// u_blueNoiseTexture: 2D, unit 7, RGBA tiling blue noise at 4 frequencies
// u_extraNoiseTexture: 3D, unit 8, R=Ridge/G=Gradient

#ifndef NOISE_TEXTURES_DEFINED
#define NOISE_TEXTURES_DEFINED
uniform sampler3D u_noiseTexture;
uniform sampler3D u_curlTexture;
uniform sampler2D u_blueNoiseTexture;
uniform sampler3D u_extraNoiseTexture;
uniform sampler2D u_phasorTexture;
#endif

// R: Simplex 3D
float fastSimplex3d(vec3 p) {
	return textureLod(u_noiseTexture, p, 0.0).r * 2.0 - 1.0;
}

// G: Worley 3D
float fastWorley3d(vec3 p) {
	return textureLod(u_noiseTexture, p, 0.0).g;
}

// B: FBM 3D
float fastFbm3d(vec3 p) {
	return textureLod(u_noiseTexture, p, 0.0).b * 2.0 - 1.0;
}

// A: Warped FBM 3D
float fastWarpedFbm3d(vec3 p) {
	return textureLod(u_noiseTexture, p, 0.0).a * 2.0 - 1.0;
}

// Extra Noises (from u_extraNoiseTexture)
// R: Ridge 3D
float fastRidge3d(vec3 p) {
	return textureLod(u_extraNoiseTexture, p, 0.0).r;
}

// G: Gradient 3D
float fastGradient3d(vec3 p) {
	return textureLod(u_extraNoiseTexture, p, 0.0).g * 2.0 - 1.0;
}

// Multi-octave texture FBM
float fastTextureFbm(vec3 p, int octaves) {
	float value = 0.0;
	float amplitude = 0.5;
	for (int i = 0; i < octaves; i++) {
		value += amplitude * (textureLod(u_noiseTexture, p, 0.0).r * 2.0 - 1.0);
		p *= 2.0;
		amplitude *= 0.5;
	}
	return value;
}

// Curl Noise lookup
vec3 fastCurl3d(vec3 p) {
	return textureLod(u_curlTexture, p, 0.0).rgb;
}

// FBM Curl magnitude lookup
float fastFbmCurl3d(vec3 p) {
	return textureLod(u_curlTexture, p, 0.0).a;
}

// Blue Noise lookups (at different frequencies)
float fastBlueNoise(vec2 uv, int frequencyIndex) {
	vec4 bn = textureLod(u_blueNoiseTexture, uv, 0.0);
	if (frequencyIndex == 0)
		return bn.r;
	if (frequencyIndex == 1)
		return bn.g;
	if (frequencyIndex == 2)
		return bn.b;
	return bn.a;
}

float fastBlueNoise(vec2 uv) {
	return textureLod(u_blueNoiseTexture, uv, 0.0).r;
}

// Spatiotemporal Blue Noise lookup using golden ratio shift
// Useful for Monte Carlo integration across frames
float fastSpatiotemporalBlueNoise(vec2 uv, int frequencyIndex, int frameIndex) {
    float bn = fastBlueNoise(uv, frequencyIndex);
    // Golden ratio = 0.61803398875
    return fract(bn + float(frameIndex) * 0.61803398875);
}

// float fastSpatiotemporalBlueNoise(vec2 uv, int frameIndex) {
// 	return fastSpatiotemporalBlueNoise(uv, 0, frameIndex);
// }

vec4 fastSpatiotemporalBlueNoise(vec2 uv, int frameIndex) {
	ivec2 bnSize = textureSize(u_blueNoiseTexture, 0);
	vec2 bnUV = (uv + vec2(frameIndex * 13, frameIndex * 7)) / vec2(bnSize);
	vec4 bn = textureLod(u_blueNoiseTexture, bnUV, 0.0);
    // float bn = fastBlueNoise(uv, frequencyIndex);
    // Golden ratio = 0.61803398875
    // return fract(bn + vec4(2.0*sin(frameIndex*0.5)  * 0.61803398875));
    return fract(bn + vec4(frameIndex)  * 0.61803398875);
}

/**
 * Fast 2D Phasor noise lookup.
 * Performs complex multiplication of baked phasor with runtime phase.
 * Returns the real part of the resulting complex number.
 */
float fastPhasor2d(vec2 uv, float runtimePhase) {
	vec2 baked = textureLod(u_phasorTexture, uv, 0.0).rg;

	// Complex multiplication: (R_baked + i*I_baked) * (cos(phi) + i*sin(phi))
	// Result real part = R_baked * cos(phi) - I_baked * sin(phi)
	float cosPhi = cos(runtimePhase);
	float sinPhi = sin(runtimePhase);

	return baked.x * cosPhi - baked.y * sinPhi;
}

#endif // HELPERS_FAST_NOISE_GLSL
#endif // GSHADERS_HELPERS_FAST_NOISE_GLSL
//END shaders/helpers/fast_noise.glsl (returning to shaders/fire.frag)
//START shaders/helpers/noise.glsl
#ifndef GSHADERS_HELPERS_NOISE_GLSL
#define GSHADERS_HELPERS_NOISE_GLSL
#ifndef HELPERS_NOISE_GLSL
#define HELPERS_NOISE_GLSL

//
// Description : Array and textureless GLSL 2D/3D simplex noise function.
//      Author : Ian McEwan, Ashima Arts.
//  Maintainer : stegu
//     Lastmod : 20110822 (ijm)
//     License : Copyright (C) 2011 Ashima Arts. All rights reserved.
//               Distributed under the MIT License. See LICENSE file.
//               https://github.com/ashima/webgl-noise
//               https://github.com/stegu/webgl-noise
//

#ifndef FNC_MOD289
	#define FNC_MOD289

float mod289(const in float x) {
	return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec2 mod289(const in vec2 x) {
	return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec3 mod289(const in vec3 x) {
	return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 mod289(const in vec4 x) {
	return x - floor(x * (1.0 / 289.0)) * 289.0;
}
#endif

#ifndef FNC_PERMUTE
	#define FNC_PERMUTE

float permute(const in float v) {
	return mod289(((v * 34.0) + 1.0) * v);
}

vec2 permute(const in vec2 v) {
	return mod289(((v * 34.0) + 1.0) * v);
}

vec3 permute(const in vec3 v) {
	return mod289(((v * 34.0) + 1.0) * v);
}

vec4 permute(const in vec4 v) {
	return mod289(((v * 34.0) + 1.0) * v);
}
#endif

#ifndef FNC_TAYLORINVSQRT
	#define FNC_TAYLORINVSQRT

float taylorInvSqrt(const in float r) {
	return 1.79284291400159 - 0.85373472095314 * r;
}

vec4 taylorInvSqrt(const in vec4 r) {
	return 1.79284291400159 - 0.85373472095314 * r;
}
#endif

vec2 fade(vec2 t) {
	return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

#ifndef FNC_SNOISE
float snoise(vec2 v) {
	const vec4 C = vec4(
		0.211324865405187,  // (3.0-sqrt(3.0))/6.0
		0.366025403784439,  // 0.5*(sqrt(3.0)-1.0)
		-0.577350269189626, // -1.0 + 2.0 * C.x
		0.024390243902439
	); // 1.0 / 41.0
	   // First corner
	vec2 i = floor(v + dot(v, C.yy));
	vec2 x0 = v - i + dot(i, C.xx);

	// Other corners
	vec2 i1;
	// i1.x = step( x0.y, x0.x ); // x0.x > x0.y ? 1.0 : 0.0
	// i1.y = 1.0 - i1.x;
	i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
	// x0 = x0 - 0.0 + 0.0 * C.xx ;
	// x1 = x0 - i1 + 1.0 * C.xx ;
	// x2 = x0 - 1.0 + 2.0 * C.xx ;
	vec4 x12 = x0.xyxy + C.xxzz;
	x12.xy -= i1;

	// Permutations
	i = mod289(i); // Avoid truncation effects in permutation
	vec3 p = permute(permute(i.y + vec3(0.0, i1.y, 1.0)) + i.x + vec3(0.0, i1.x, 1.0));

	vec3 m = max(0.5 - vec3(dot(x0, x0), dot(x12.xy, x12.xy), dot(x12.zw, x12.zw)), 0.0);
	m = m * m;
	m = m * m;

	// Gradients: 41 points uniformly over a unit circle.
	// The ring size 17*17 = 289 is close to a multiple of 41 (41*7 = 287)
	vec3 x = 2.0 * fract(p * C.www) - 1.0;
	vec3 h = abs(x) - 0.5;
	vec3 ox = floor(x + 0.5);
	vec3 a0 = x - ox;

	// Normalization factor
	m *= 1.79284291400159 - 0.85373472095314 * (a0 * a0 + h * h);

	// Compute final noise value at 2D position
	vec3 g;
	g.x = a0.x * x0.x + h.x * x0.y;
	g.yz = a0.yz * x12.xz + h.yz * x12.yw;
	return 130.0 * dot(m, g);
}
#endif

float snoise3d(vec3 v) {
	const vec2 C = vec2(1.0 / 6.0, 1.0 / 3.0);
	const vec4 D = vec4(0.0, 0.5, 1.0, 2.0);

	// First corner
	vec3 i = floor(v + dot(v, C.yyy));
	vec3 x0 = v - i + dot(i, C.xxx);

	// Other corners
	vec3 g = step(x0.yzx, x0.xyz);
	vec3 l = 1.0 - g;
	vec3 i1 = min(g.xyz, l.zxy);
	vec3 i2 = max(g.xyz, l.zxy);

	vec3 x1 = x0 - i1 + C.xxx;
	vec3 x2 = x0 - i2 + C.yyy; // 2.0*C.x = 1/3 = C.y
	vec3 x3 = x0 - D.yyy;

	// Permutations
	i = mod289(i);
	vec4 p = permute(
		permute(permute(i.z + vec4(0.0, i1.z, i2.z, 1.0)) + i.y + vec4(0.0, i1.y, i2.y, 1.0)) + i.x +
		vec4(0.0, i1.x, i2.x, 1.0)
	);

	float n_ = 0.142857142857;
	vec3  ns = n_ * D.wyz - D.xzx;

	vec4 j = p - 49.0 * floor(p * ns.z * ns.z);

	vec4 x_ = floor(j * ns.z);
	vec4 y_ = floor(j - 7.0 * x_);

	vec4 x = x_ * ns.x + ns.yyyy;
	vec4 y = y_ * ns.x + ns.yyyy;
	vec4 h = 1.0 - abs(x) - abs(y);

	vec4 b0 = vec4(x.xy, y.xy);
	vec4 b1 = vec4(x.zw, y.zw);

	vec4 s0 = floor(b0) * 2.0 + 1.0;
	vec4 s1 = floor(b1) * 2.0 + 1.0;
	vec4 sh = -step(h, vec4(0.0));

	vec4 a0 = b0.xzyw + s0.xzyw * sh.xxyy;
	vec4 a1 = b1.xzyw + s1.xzyw * sh.zzww;

	vec3 p0 = vec3(a0.xy, h.x);
	vec3 p1 = vec3(a0.zw, h.y);
	vec3 p2 = vec3(a1.xy, h.z);
	vec3 p3 = vec3(a1.zw, h.w);

	// Normalise gradients
	vec4 norm = taylorInvSqrt(vec4(dot(p0, p0), dot(p1, p1), dot(p2, p2), dot(p3, p3)));
	p0 *= norm.x;
	p1 *= norm.y;
	p2 *= norm.z;
	p3 *= norm.w;

	// Mix final noise value
	vec4 m = max(0.6 - vec4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.0);
	m = m * m;
	return 42.0 * dot(m * m, vec4(dot(p0, x0), dot(p1, x1), dot(p2, x2), dot(p3, x3)));
}

/**
 * 2D Curl Noise for wind effects.
 * Calculates the curl of a 2D simplex noise field.
 */
vec2 curlNoise2D(vec2 p) {
	const float e = 0.1;
	float       n1 = snoise(vec2(p.x, p.y + e));
	float       n2 = snoise(vec2(p.x, p.y - e));
	float       n3 = snoise(vec2(p.x + e, p.y));
	float       n4 = snoise(vec2(p.x - e, p.y));

	float dy = (n1 - n2) / (2.0 * e);
	float dx = (n3 - n4) / (2.0 * e);

	return vec2(dy, -dx);
}

// A simple 3D hash function (returns pseudo-random vec3 between 0 and 1)
#ifndef FNC_HASH33
	#define FNC_HASH33

vec3 hash33(vec3 p) {
	p = fract(p * vec3(443.897, 441.423, 437.195));
	p += dot(p, p.yxz + 19.19);
	return fract((p.xxy + p.yxx) * p.zyx);
}
#endif

#ifndef FNC_HASH22
	#define FNC_HASH22
vec2 hash22(vec2 p) {
	vec3 p3 = fract(vec3(p.xyx) * vec3(443.897, 441.423, 437.195));
	p3 += dot(p3, p3.yzx + 19.19);
	return fract((p3.xx + p3.yz) * p3.zy);
}
#endif

#endif // HELPERS_NOISE_GLSL
#endif // GSHADERS_HELPERS_NOISE_GLSL
//END shaders/helpers/noise.glsl (returning to shaders/fire.frag)

// Robust polynomial fit for HDR-friendly fire
vec3 blackbody_hdr(float t) {
	vec3 col;
	// Red kicks in immediately and saturates quickly
	col.r = smoothstep(0.0, 0.2, t);

	// Green starts earlier for more orange and yellow
	col.g = smoothstep(0.02, 0.4, t);

	// Blue for the core hot spot
	col.b = smoothstep(0.3, 0.8, t);

	// Hollywood stunt fire: Rich orange/yellow boost
	return col * vec3(8.0, 2.5, 1.5);
}

float turbulence(vec2 p) {
	return fastRidge3d(vec3(p, u_time));
}

const float kExhaustLifetime = 2.0;
const float kExplosionLifetime = 2.5;
const float kFireLifetime = 5.0;
const float kSparksLifetime = 0.8;
const float kGlitterLifetime = 3.5;

void main() {
	// Shape the point into a circle and discard fragments outside the circle
	vec2  circ = gl_PointCoord - vec2(0.5);
	float distSq = dot(circ, circ);
	float shapeMask = smoothstep(0.25, 0.1, distSq);

	vec3  color = vec3(0.0);
	float alpha = 0.0;

	if (v_style == STYLE_ROCKET_TRAIL || v_style == STYLE_SPARKS || v_style == STYLE_GLITTER || v_style == STYLE_BUBBLES || v_style == STYLE_FIREFLIES || v_style == STYLE_DEBUG ||
	    v_style == STYLE_CINDER || v_style == STYLE_IRIDESCENT || v_style == STYLE_RAIN || v_style == STYLE_SNOW || v_style == STYLE_LEAF || v_style == STYLE_PETAL || v_style == STYLE_BIRDS || v_style == STYLE_FAIRY) {

		color = v_p.color.rgb;
		alpha = v_p.color.a;

		if (v_style == STYLE_LEAF || v_style == STYLE_PETAL || v_style == STYLE_FIREFLIES || v_style == STYLE_BIRDS || v_style == STYLE_FAIRY) {
			vec3 biome_albedo = (v_emitter_index >= 0 && v_emitter_index < 8) ? u_biomeAlbedos[v_emitter_index] : vec3(0.5);
			color = mix(color, biome_albedo, 0.5);
		}

		if (v_style == STYLE_BUBBLES) {
			vec3 n; n.xy = circ * 2.0;
			float magSq = dot(n.xy, n.xy);
			n.z = sqrt(max(0.0, 1.0 - magSq));
			float fresnel = pow(max(0.0, 1.0 - n.z), 3.0);
			float swirl = sin(v_lifetime * 2.0 + gl_PointCoord.y * 5.0) * 0.5 + 0.5;
			vec3 iridescent_color = vec3(sin(swirl * 5.0) * 0.5 + 0.5, sin(swirl * 5.0 + 2.0) * 0.5 + 0.5, sin(swirl * 5.0 + 4.0) * 0.5 + 0.5);
			vec3 l = normalize(vec3(0.5, 0.5, 1.0));
			vec3 h = normalize(l + vec3(0, 0, 1));
			float spec = pow(max(dot(n, h), 0.0), 64.0);
			color = mix(iridescent_color, vec3(1.0), fresnel * 0.5 + 0.2) + spec;
		} else if (v_style == STYLE_SNOW) {
			float r = 0.5 * length(circ);
			float a = atan(circ.y, circ.x);
			float s = abs(sin(a * 3.0));
			shapeMask = smoothstep(0.1, 0.09, r * s);
		} else if (v_style == STYLE_RAIN) {
			vec2 vel_dir = normalize(v_vel_view.xy + vec2(1e-6));
			float angle = atan(vel_dir.y, vel_dir.x) + 1.5707;
			mat2 rot = mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
			vec2 uv = (gl_PointCoord - 0.5) * rot + 0.5;
			float y = clamp(uv.y, 0.0, 1.0);
			float width = mix(0.02, 0.15, y);
			float streak = smoothstep(width*0.25, width * 0.05, abs(uv.x - 0.5)) * smoothstep(0.0, 0.2, uv.y) * smoothstep(1.0, 0.8, uv.y);
			alpha *= streak;
			color = vec3(0.2, 0.3, 0.5);
			shapeMask = 1.0;
		} else if (v_style == STYLE_IRIDESCENT) {
			float fresnel = pow(max(0.0, 1.0 - distSq * 4.0), 5.0);
			float angle_factor = pow(clamp(1.0 - distSq * 4.0, 0.0, 1.0), 2.0);
			float swirl = sin(v_lifetime * 0.5 + gl_PointCoord.y * 2.0) * 0.5 + 0.5;
			vec3 iridescent_color = vec3(sin(angle_factor * 10.0 + swirl * 5.0) * 0.5 + 0.5, sin(angle_factor * 10.0 + swirl * 5.0 + 2.0) * 0.5 + 0.5, sin(angle_factor * 10.0 + swirl * 5.0 + 4.0) * 0.5 + 0.5);
			vec3 view_dir = length(view_pos.xyz) > 0.001 ? normalize(-view_pos.xyz) : vec3(0, 0, 1);
			vec3 r = reflect(-view_dir, vec3(0, 1, 0));
			float spec = pow(max(dot(view_dir, r), 0.0), 64.0);
			color = mix(iridescent_color, vec3(1.0), fresnel) + 1.5 * spec * vec3(1.0);
		} else if (v_style == STYLE_CINDER) {
			float n = snoise3d(vec3(gl_PointCoord * 6.0, float(v_particle_idx)));
			shapeMask = smoothstep(0.2 + n * 0.15, 0.05, distSq);
		} else if (v_style == STYLE_BIRDS) {
			float flap = sin(u_time * 15.0 + v_p.phase);
			float wing_y = abs(gl_PointCoord.x - 0.5) * (0.5 + flap * 0.4);
			float body = (1-smoothstep(0.0, 0.1, abs(gl_PointCoord.y - 0.5 - wing_y)) + abs(gl_PointCoord.x - 0.7) * 0.5);
			body *= step(0.70, body);
			shapeMask = body;
			color = mix(
				mix(color, vec3(0.5, 0.8, 0.3), 0.43),
				mix(color * 2.0, vec3(0.2, 0.9, 0.9), 0.63),
				smoothstep(0.30, 0.40, gl_PointCoord.x) * (1.0 - smoothstep(0.60, 0.70, gl_PointCoord.x))
			);
			alpha = 1.0;
		} else if (v_style == STYLE_FAIRY) {
			// Phase-based brightness (using p.counter and p.phase logic from firefly)
			float twinkle = pow(smoothstep(0.0, 0.3, v_p.counter) * (1.0 - smoothstep(0.4, 0.6, v_p.counter)), 2.0) * step(v_p.counter, 0.6);
			float brightness = 0.5 + twinkle * 5.0;

			// Small soft core
			float core = exp(-distSq * 100.0);
			// Large penumbra
			float penumbra = exp(-distSq * 15.0);

			// Iridescent sheen in the penumbra
			float angle_factor = pow(clamp(1.0 - distSq * 4.0, 0.0, 1.0), 2.0);
			vec3 iridescent_color = vec3(
				sin(angle_factor * 8.0 + v_p.phase) * 0.5 + 0.5,
				sin(angle_factor * 8.0 + v_p.phase + 2.0) * 0.5 + 0.5,
				sin(angle_factor * 8.0 + v_p.phase + 4.0) * 0.5 + 0.5
			);

			// Glittering sparkles using Worley noise
			float sparkle = pow(fastWorley3d(v_pos.xyz * 10.0 + u_time * 2.0), 16.0);
			vec3 sparkle_color = vec3(1.0, 0.9, 0.6) * sparkle * 10.0;

			color = mix(vec3(1.0, 1.0, 1.0), iridescent_color, 0.7) * penumbra;
			color += core * 2.0;
			color += sparkle_color * penumbra;
			color *= brightness;

			shapeMask = penumbra;
			alpha = shapeMask * (0.4 + 0.6 * twinkle);
		}

		alpha *= shapeMask;
		color *= alpha;
	} else {
		float maxLife = (v_style == STYLE_EXPLOSION) ? kExplosionLifetime : kFireLifetime;
		float distFromEpicenter = length(v_pos.xyz - v_origin);
		float normalizedLife = clamp(v_lifetime / maxLife, 0.0, 1.0);
		float roilScale = (v_style == STYLE_EXPLOSION) ? 0.015 : 0.03;
		float roil = fastFbm3d(v_pos.xyz * roilScale - vec3(0.0, u_time * 0.1, 0.0)) * 0.5 + 0.5;
		float worleyScale = (v_style == STYLE_EXPLOSION) ? 0.05 : 0.1;
		float knobly = fastWorley3d(v_pos.xyz * worleyScale * (1.0 + distFromEpicenter * 0.03) + vec3(u_time * 0.05));
		float noiseDetail = mix(roil, knobly, abs(fastSimplex3d(vec3(0, u_time, 0))));
		noiseDetail = mix(noiseDetail, noiseDetail * (fastSimplex3d(vec3(gl_PointCoord * 0.4, u_time * 0.05)) * 0.5 + 0.5), 0.35);
		float heat = normalizedLife * pow(noiseDetail, 1.4) * pow(max(0.0, 1.0 - (distSq * 4.0)), 0.7) * ((v_style == STYLE_EXPLOSION) ? smoothstep(80.0, 0.0, distFromEpicenter) : 1.0);
		alpha = shapeMask * smoothstep(0.01, 0.12, heat) * turbulence(gl_PointCoord);
		color = blackbody_hdr(heat) * alpha * 12.0 * (1.0 + normalizedLife);
	}

	// Apply atmospheric scattering and fog to particles
	// Note: v_pos.xyz is world position
	float depth = length(view_pos.xyz);

	float transmittance = 1.0;
	vec3 scattering = vec3(1.0);

#ifdef ATMOSPHERE_COMMON_GLSL
	// Calculate atmosphere properties at particle position
	Sampling s = getAtmospherePropertiesAtPos(v_pos.xyz);

	// Physically-based fogging:
	// 1. Transmittance attenuates the particle's own emission
	// 2. Scattering adds the atmosphere's own glow between camera and particle
	transmittance = exp(-length(s.extinction) * (depth / 1000.0)); // Convert meters to KM for extinction lookup
	scattering = ambient_light * (1.0 - transmittance);
	color = color * transmittance;
#endif

	// Dual exposure/lighting fix:
	// Ambient particles get standard lighting, while emissive ones get a boost.
	if (v_style == STYLE_ROCKET_TRAIL || v_style == STYLE_FIRE || v_style == STYLE_EXPLOSION || v_style == STYLE_SPARKS || v_style == STYLE_GLITTER || v_style == STYLE_FIREFLIES) {
		// Emissive/self-lit particles are already bright enough.
		// For fire (additive), we only apply transmittance to the color.
		// We don't add ambient scattering directly as it would make the fire look like a solid block in fog.
		// Instead, we let the scattering affect the scene behind it.
	} else {
		// Ambient particles (leaves, petals, birds, etc.) should receive scene ambient.
		vec3 ambient = sh_coeffs[0].xyz * 0.5 + 0.5; // Simple approximation of global ambient
		color *= ambient * (1.0 + nightFactor);
	}

	FragColor = vec4(color, alpha);
}
#endif // GSHADERS_FIRE_FRAG
