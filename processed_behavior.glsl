#ifndef GSHADERS_PARTICLE_BEHAVIOR_GLSL
#define GSHADERS_PARTICLE_BEHAVIOR_GLSL
#ifndef PARTICLE_BEHAVIOR_GLSL
#define PARTICLE_BEHAVIOR_GLSL

//START shaders/helpers/spatial_hash.glsl
#ifndef GSHADERS_HELPERS_SPATIAL_HASH_GLSL
#define GSHADERS_HELPERS_SPATIAL_HASH_GLSL
#ifndef SPATIAL_HASH_GLSL
#define SPATIAL_HASH_GLSL

uint get_cell_idx(vec3 pos, float cellSize, uint gridSize) {
	ivec3 cellPos = ivec3(floor(pos / cellSize));
	// Spatial hash using large primes
	uint h = uint(cellPos.x * 73856093) ^ uint(cellPos.y * 19349663) ^ uint(cellPos.z * 83492791);
	return h % gridSize;
}

#endif
#endif // GSHADERS_HELPERS_SPATIAL_HASH_GLSL
//END shaders/helpers/spatial_hash.glsl (returning to shaders/particle_behavior.glsl)
//START shaders/particle_helpers.glsl
#ifndef GSHADERS_PARTICLE_HELPERS_GLSL
#define GSHADERS_PARTICLE_HELPERS_GLSL
#ifndef PARTICLE_HELPERS_GLSL
#define PARTICLE_HELPERS_GLSL

// Basic pseudo-random number generator
float rand(vec2 co) {
	return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

// 3D random number generator
vec3 rand3(vec2 co) {
	return vec3(rand(co + vec2(0.1, 0.2)), rand(co + vec2(0.3, 0.4)), rand(co + vec2(0.5, 0.6)));
}

// A standard 32-bit integer hash
uint hash(uint x) {
	x ^= x >> 16;
	x *= 0x7feb352dU;
	x ^= x >> 15;
	x *= 0x846ca68bU;
	x ^= x >> 16;
	return x;
}

// Convert the hashed integer to a float in [0.0, 1.0]
float randomFloat(uint state) {
	return float(hash(state)) / 4294967295.0; // Divide by 0xFFFFFFFF
}

// Calculate Curl Noise by sampling the pre-calculated curl texture
vec3 curlNoise(vec3 p, float time, sampler3D curlTexture) {
	float noiseScale = 0.02;
	vec3  lookupPos = p * noiseScale + vec3(0, 0, time * 0.1);
	return texture(curlTexture, lookupPos).rgb;
}

float fbmCurlMagnitude(vec3 p, float time, sampler3D curlTexture) {
	float noiseScale = 0.02;
	vec3  lookupPos = p * noiseScale + vec3(0, 0, time * 0.1);
	return texture(curlTexture, lookupPos).a;
}

#endif
#endif // GSHADERS_PARTICLE_HELPERS_GLSL
//END shaders/particle_helpers.glsl (returning to shaders/particle_behavior.glsl)
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
//END shaders/particle_types.glsl (returning to shaders/particle_behavior.glsl)
//START shaders/visual_effects.glsl
#ifndef GSHADERS_VISUAL_EFFECTS_GLSL
#define GSHADERS_VISUAL_EFFECTS_GLSL
#ifndef VISUAL_EFFECTS_GLSL
#define VISUAL_EFFECTS_GLSL

layout(std140, binding = 1) uniform VisualEffects {
	int   ripple_enabled;
	int   color_shift_enabled;
	int   black_and_white_enabled;
	int   negative_enabled;
	int   shimmery_enabled;
	int   glitched_enabled;
	int   wireframe_enabled;
	int   erosion_enabled;
	float wind_strength;
	float wind_speed;
	float wind_frequency;
	float erosion_strength;
	float erosion_scale;
	float erosion_detail;
	float erosion_gully_weight;
	float erosion_max_dist;
	float rain_intensity;
	float snow_intensity;
	float wetness;
	float temperature;
};

#endif
#endif // GSHADERS_VISUAL_EFFECTS_GLSL
//END shaders/visual_effects.glsl (returning to shaders/particle_behavior.glsl)
//START shaders/helpers/wind.glsl
#ifndef GSHADERS_HELPERS_WIND_GLSL
#define GSHADERS_HELPERS_WIND_GLSL
#ifndef HELPERS_WIND_GLSL
#define HELPERS_WIND_GLSL

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
//END shaders/helpers/fast_noise.glsl (returning to shaders/helpers/wind.glsl)
//START shaders/helpers/terrain_common.glsl
#ifndef GSHADERS_HELPERS_TERRAIN_COMMON_GLSL
#define GSHADERS_HELPERS_TERRAIN_COMMON_GLSL
#ifndef HELPERS_TERRAIN_COMMON_GLSL
#define HELPERS_TERRAIN_COMMON_GLSL

#ifndef TERRAIN_DATA_BLOCK
#define TERRAIN_DATA_BLOCK
layout(std140, binding = 8) uniform TerrainData {
	ivec4 u_originSize;    // x, z, size, is_bound
	vec4  u_terrainParams; // chunkSize, worldScale
};
#endif

#ifndef TERRAIN_GRID_DEFINED
	#define TERRAIN_GRID_DEFINED
layout(binding = 11) uniform isampler2D u_chunkGrid;
#endif

layout(binding = 12) uniform sampler2D  u_maxHeightGrid;
layout(binding = 13) uniform sampler2DArray u_heightmapArray;

/**
 * Get the terrain height at a specific world position.
 */
float getTerrainHeight(vec2 worldXZ) {
	if (u_originSize.w < 1)
		return -10000.0;
	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;
	vec2  gridPos = worldXZ / scaledChunkSize;
	ivec2 chunkCoord = ivec2(floor(gridPos));
	ivec2 localGridCoord = chunkCoord - u_originSize.xy;

	if (localGridCoord.x < 0 || localGridCoord.x >= u_originSize.z || localGridCoord.y < 0 ||
	    localGridCoord.y >= u_originSize.z) {
		return -9999.0;
	}

	int slice = texelFetch(u_chunkGrid, localGridCoord, 0).r;
	if (slice < 0)
		return -10000.0;

	vec2 uv = (worldXZ - vec2(chunkCoord) * scaledChunkSize) / scaledChunkSize;
	vec2 remappedUV = (uv * u_terrainParams.x + 0.5) / (u_terrainParams.x + 1.0);
	return texture(u_heightmapArray, vec3(remappedUV, float(slice))).r;
}

/**
 * Get the terrain normal at a specific world position.
 */
vec3 getTerrainNormal(vec2 worldXZ) {
	if (u_originSize.w < 1)
		return vec3(0.0, 1.0, 0.0);
	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;
	vec2  gridPos = worldXZ / scaledChunkSize;
	ivec2 chunkCoord = ivec2(floor(gridPos));
	ivec2 localGridCoord = chunkCoord - u_originSize.xy;

	if (localGridCoord.x < 0 || localGridCoord.x >= u_originSize.z || localGridCoord.y < 0 ||
	    localGridCoord.y >= u_originSize.z) {
		return vec3(0.0, 1.0, 0.0);
	}

	int slice = texelFetch(u_chunkGrid, localGridCoord, 0).r;
	if (slice < 0)
		return vec3(0.0, 1.0, 0.0);

	vec2 uv = (worldXZ - vec2(chunkCoord) * scaledChunkSize) / scaledChunkSize;
	vec2 remappedUV = (uv * u_terrainParams.x + 0.5) / (u_terrainParams.x + 1.0);
	// Normal is stored in GBA of the heightmap array
	return texture(u_heightmapArray, vec3(remappedUV, float(slice))).gba;
}

#endif // HELPERS_TERRAIN_COMMON_GLSL
#endif // GSHADERS_HELPERS_TERRAIN_COMMON_GLSL
//END shaders/helpers/terrain_common.glsl (returning to shaders/helpers/wind.glsl)

// Wind data UBO - stores macro wind grid and simulation parameters
#ifndef WIND_DATA_BLOCK
#define WIND_DATA_BLOCK
layout(std140, binding = 45) uniform WindData {
	ivec4 u_windOriginSize; // x, z = origin in chunks, y = size (width), w = height (60)
	vec4  u_windParams;     // x = chunkSpacing (32.0), y = time, z = curlScale, w = curlStrength
};

layout(binding = 26) uniform sampler2D u_windTexture;
#ifdef WIND_COMPUTE
layout(binding = 34) uniform sampler2D u_lbmWindTexture;
#endif

layout(binding = 42) uniform sampler2D u_weatherScalarsTexture;
layout(binding = 43) uniform sampler2D u_weatherAerosolsTexture;
#endif

/**
 * Fast lookup for pre-integrated wind data.
 */
vec3 getWindAtPosition(vec3 worldPos) {
	if (u_windOriginSize.y <= 0) return vec3(0.0);

	float gridSpacing = u_windParams.x;
	vec2 gridCoord = (worldPos.xz / gridSpacing) - vec2(u_windOriginSize.xz);
	vec2 uv = gridCoord / vec2(u_windOriginSize.y, u_windOriginSize.w);

	return texture(u_windTexture, uv).xyz;
}

/**
 * Get weather scalars (x: temperature, y: humidity, z: pressure, w: viscosityDamping)
 */
vec4 getWeatherScalarsAtPosition(vec3 worldPos) {
	if (u_windOriginSize.y <= 0) return vec4(0.0);

	float gridSpacing = u_windParams.x;
	vec2 gridCoord = (worldPos.xz / gridSpacing) - vec2(u_windOriginSize.xz);
	vec2 uv = gridCoord / vec2(u_windOriginSize.y, u_windOriginSize.w);

	return texture(u_weatherScalarsTexture, uv);
}

/**
 * Get weather aerosols (x, y, z, w: concentrations of 4 aerosol types)
 */
vec4 getWeatherAerosolsAtPosition(vec3 worldPos) {
	if (u_windOriginSize.y <= 0) return vec4(0.0);

	float gridSpacing = u_windParams.x;
	vec2 gridCoord = (worldPos.xz / gridSpacing) - vec2(u_windOriginSize.xz);
	vec2 uv = gridCoord / vec2(u_windOriginSize.y, u_windOriginSize.w);

	return texture(u_weatherAerosolsTexture, uv);
}

#ifdef WIND_COMPUTE
/**
 * Calculates the combined wind vector at a given world position.
 * Incorporates macro LBM-derived wind, terrain deflection, and small-scale curl noise.
 */
vec4 computeWindAtPosition(vec3 worldPos) {
	if (u_windOriginSize.y <= 0) return vec4(0.0);

	float gridSpacing = u_windParams.x;
	// Measurements are at cell centers, so offset by half spacing for interpolation
	vec2 gridCoord = (worldPos.xz / gridSpacing) - vec2(u_windOriginSize.xz);

	// Normalize to [0, 1] for texture sampling
	vec2 uv = gridCoord / vec2(u_windOriginSize.y, u_windOriginSize.w);

	// 1. Hardware-accelerated bilinear interpolation of macro wind and drag
	// Use the RAW LBM texture here for integration
	vec4 macroData = texture(u_lbmWindTexture, uv);

	vec3 macroWind = macroData.xyz;
	// return macroWind;
	float drag = macroData.w;
	float macroSpeed = length(macroWind);

	// 2. Terrain Guidance
	// Deflect wind based on terrain normal to follow slopes
	vec3 normal = getTerrainNormal(worldPos.xz);
	float terrainHeight = getTerrainHeight(worldPos.xz);

	// How close we are to the ground affects guidance strength
	float distToGround = max(0.0, worldPos.y - terrainHeight);
	float guidanceStrength = exp(-distToGround * 0.1); // Stronger near ground

	if (macroSpeed > 0.001) {
		vec3 windDir = macroWind / macroSpeed;
		// If wind is hitting the slope, push it along the surface
		float vDotN = dot(windDir, normal);
		if (vDotN < 0.0) {
			// Deflect: remove the component going into the terrain and normalize
			vec3 deflectedDir = normalize(windDir - vDotN * normal);
			macroWind = mix(macroWind, deflectedDir * macroSpeed, guidanceStrength);
		}
	}

	// 3. Structured Gusts and Swirls
	// Use continuous time. Do not wrap it, or the phase math will fracture.
	float time = u_windParams.y;
	float curlScale = u_windParams.z;
	float curlStrength = u_windParams.w;

	float flatTime = mix(mod(time, 29) * sin(time) * 0.5 + 0.5, mod(time * 1.07, 31)  * cos(time) * 0.5 + 0.5, sin(time) * 0.5 + 0.5);

	// Extract normalized direction for predictable math
	vec2 windDir2D = macroSpeed > 0.001 ? macroWind.xz / macroSpeed : vec2(1.0, 0.0);

	// Gustiness: Smoothstep the simplex to create wide, rolling "valleys" of stillness
	float gustAdvectionSpeed = 0.75;
	vec3 gustPos = worldPos - (macroWind * time * gustAdvectionSpeed);
	float gustiness = smoothstep(0.1, 0.7, fastSimplex3d(gustPos / 250.0) * 0.5 + 0.5);

	// 4. Phasor Ripples (The "Packets")
	// Frequency and phase speed adapt to macro speed to prevent high-frequency chaos.
	// As speed increases, we widen the ripples and slow down their oscillation.
	float speedSmoothing = 1.0 / (1.0 + macroSpeed * 0.05);

	float rippleFreq = 0.005 * speedSmoothing;
	vec2 rippleUV = worldPos.xz * rippleFreq;

	float rippleTightness = 0.05;
	float ripplePhaseSpeed = 5.0 * speedSmoothing;

	float phaseShift = dot(windDir2D, worldPos.xz) * rippleTightness - (time * ripplePhaseSpeed);
	float rawPhasor = fastPhasor2d(rippleUV, phaseShift);

	// Remap and apply an asymmetric power curve to fix the "pulling" vertex snap-back
	// This makes the gust hit quickly, but release slowly.
	float positiveRipple = pow(rawPhasor * 0.5 + 0.5, 2.0);

	// 5. The Stillness Filter
	// Attenuate the base wind during lulls, but ONLY if the macro wind is relatively weak.
	// Storms (high macroSpeed) will ignore the lull and blow continuously.
	float calmThreshold = smoothstep(50.0, 0.0, macroSpeed);
	float baseWindMultiplier = mix(1.0, gustiness, calmThreshold);

	vec3 finalWind = macroWind * baseWindMultiplier;

	// 6. Apply the Gust Surge
	if (macroSpeed > 0.001) {
		float surgeStrength = 2.5;
		// The surge only exists inside the macro gusts
		float localizedSurge = positiveRipple * gustiness * surgeStrength * macroSpeed;
		finalWind.xz += windDir2D * localizedSurge;
	}

	// 7. Local Turbulence (Curl)
	// Scale and temporal drift also adapt to macro speed for smoother transitions at high intensity.
	float dynamicCurlScale = curlScale * (0.8 + 0.4 * gustiness) * speedSmoothing;
	float pulsePeriod = 10.0;

	float phase0 = fract(time / pulsePeriod);
	float time0 = phase0 * pulsePeriod;
	vec3 advectedPos0 = worldPos - (finalWind * time0 * 0.250);
	vec3 curl0 = fastCurl3d(advectedPos0 / 200.0 * dynamicCurlScale + vec3(0.0, time0 * 0.02 * speedSmoothing, 0.0));
	float weight0 = sin(phase0 * 3.14159);

	float phase1 = fract((time + pulsePeriod * 0.5) / pulsePeriod);
	float time1 = phase1 * pulsePeriod;
	vec3 advectedPos1 = worldPos - (finalWind * time1 * 0.250);
	vec3 curl1 = fastCurl3d(advectedPos1 / 200.0 * dynamicCurlScale + vec3(0.0, time1 * 0.02 * speedSmoothing, 0.0));
	float weight1 = sin(phase1 * 3.14159);

	vec3 curl = (curl0 * weight0 + curl1 * weight1) / (weight0 + weight1);

	float turbulenceIntensity = drag * length(finalWind) * curlStrength * speedSmoothing;

	// Modulate turbulence intensity and introduce a subtle directional shift to the flow
	turbulenceIntensity *= (0.8 + 0.4 * (positiveRipple * 0.5 + 0.5));
	if (macroSpeed > 0.001) {
		// Apply a small perpendicular shift based on the ripple
		vec2 perpWind = vec2(-macroWind.z, macroWind.x) / macroSpeed;
		macroWind.xz += perpWind * (positiveRipple * macroSpeed * 0.15);
	}

	// 5. Combined Result
	// Instead of just adding curl, we use it to perturb the direction of the macro wind,
	// creating the effect of chaotic swirls within the flow.
	if (macroSpeed > 0.001) {
		// Use curl to rotate the macro wind vector slightly
		vec3 rotationAxis = normalize(curl + vec3(0.0, 1.0, 0.0)); // Bias axis toward Up
		float rotationAngle = turbulenceIntensity * 0.15;

		float cosTheta = cos(rotationAngle);
		float sinTheta = sin(rotationAngle);

		macroWind = macroWind * cosTheta +
					cross(rotationAxis, macroWind) * sinTheta +
					rotationAxis * dot(rotationAxis, macroWind) * (1.0 - cosTheta);
	}

	// Add curl as a final perturbation
	return vec4(finalWind + curl * turbulenceIntensity, drag);
}
#endif

#endif // HELPERS_WIND_GLSL
#endif // GSHADERS_HELPERS_WIND_GLSL
//END shaders/helpers/wind.glsl (returning to shaders/particle_behavior.glsl)
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
//END shaders/helpers/noise.glsl (returning to shaders/particle_behavior.glsl)

// Simulation parameters
const float kExhaustSpeed = 30.0;
const float kExhaustSpread = 0.1;
const float kExhaustLifetime = 2.0;
const float kExhaustDrag = 1.5;

const float kExplosionSpeed = 30.0;
const float kExplosionLifetime = 2.5;
const float kExplosionDrag = 2.0;

const float kFireSpeed = 0.5;
const float kFireSpread = 4.2;
const float kFireLifetime = 5.0;

const float kSparksSpeed = 40.0;
const float kSparksLifetime = 0.8;
const float kSparksDrag = 1.0;
const float kSparksGravity = 15.0;

const float kGlitterSpeed = 25.0;
const float kGlitterSpread = 0.4;
const float kGlitterLifetime = 3.5;
const float kGlitterDrag = 1.2;
const float kGlitterGravity = 4.0;

const float kCinderSpeed = 1.0;
const float kCinderLifetime = 8.0;
const float kCinderDriftIntensity = 4.0;
const float kCinderBuoyancy = 1.5;

bool handleTerrainCollision(inout Particle p, int num_chunks, sampler2DArray heightmapArray) {
	bool collided = false;
	for (int i = 0; i < num_chunks; i++) {
		ChunkInfo chunk = chunks[i];
		if (p.pos.x >= chunk.worldOffset.x && p.pos.x < chunk.worldOffset.x + chunk.size &&
		    p.pos.z >= chunk.worldOffset.y && p.pos.z < chunk.worldOffset.y + chunk.size) {
			vec2  uv = (p.pos.xz - chunk.worldOffset) / chunk.size;
			vec4  terrain = texture(heightmapArray, vec3(uv, chunk.slice));
			float height = terrain.r;
			vec3  normal = vec3(terrain.g, terrain.b, terrain.a);

			if (p.pos.y < height) {
				p.pos.y = height + 0.05;
				p.vel.xyz = reflect(p.vel.xyz, normal) * 0.4;
				collided = true;
			}
			break;
		}
	}

	if (!collided && p.pos.y <= 0.0) {
		p.pos.y = 0.1;
		p.vel.y *= -0.25;
		p.vel.xz *= 0.8;
	}

	return collided;
}

void applyAmbientAvoidance(inout Particle p, float dt, float time, vec3 viewPos, vec3 viewDir, sampler3D curlTexture) {
	float mildRepulsionStrength = 5.0;
	float capsuleWidth = 50.0;
	float noiseWeight = 0.5;
	float pushStrength = 20.0;

	vec3  relativePos = p.pos.xyz - viewPos;
	float distToCam = length(relativePos);
	vec3  camToParticle = distToCam > 0.001 ? relativePos / distToCam : vec3(0.0, 1.0, 0.0);
	float projection = dot(relativePos, viewDir);
	vec3  rejectDir = relativePos - (projection * viewDir);
	float distToAxis = length(rejectDir);
	float sphereInfluence = smoothstep(20.0, 10.0, distToCam);
	float inwardSpeed = dot(p.vel.xyz, -camToParticle);
	float isFront = step(0.0, projection);
	float forwardFalloff = smoothstep(150.0, 0.0, distToCam);
	float axisFalloff = smoothstep(capsuleWidth, 0.0, distToAxis);
	float directionBlend = clamp(distToCam / 150.0, 0.0, 1.0);
	vec3  rejectNorm = distToAxis > 0.001 ? rejectDir / distToAxis : vec3(0.0, 1.0, 0.0);
	vec3  flatReject = vec3(rejectNorm.x, 0.0, rejectNorm.z);
	vec3  lateralPush = length(flatReject) > 0.001 ? normalize(flatReject) : vec3(1.0, 0.0, 0.0);
	vec3  terrainSafeReject = normalize(
		mix(rejectNorm, lateralPush + vec3(0, -rejectNorm.y, 0), smoothstep(0.25, -1, rejectNorm.y))
	);
	vec3 basePushDir = normalize(mix(terrainSafeReject, camToParticle, directionBlend));
	vec3 finalPushDir = normalize(basePushDir + curlNoise(p.pos.xyz, time, curlTexture) * noiseWeight);

	p.vel.xyz += step(0, inwardSpeed) * (camToParticle)*inwardSpeed * sphereInfluence;
	p.vel.xyz += camToParticle * mildRepulsionStrength * sphereInfluence;
	p.vel.xyz += finalPushDir * pushStrength * forwardFalloff * axisFalloff * isFront;
}

void updateRocketTrail(inout Particle p, float dt) {
	p.vel.xyz -= p.vel.xyz * kExhaustDrag * 2.0 * (1.0 - (length(p.vel.xyz) / 30.0)) * dt;
	p.color = vec4(0.1, 0.1, 0.1, p.pos.w * 0.4);
	p.vel.w = smoothstep(0.0, 1.0, p.pos.w / kExhaustLifetime) * 15.0;
	p.origin.w = 2.0; // Intensity
}

void updateExplosion(inout Particle p, float dt, float time, sampler3D curlTexture) {
	p.vel.xyz -= p.vel.xyz * kExplosionDrag * dt;
	float dist = distance(p.pos.xyz, p.origin.xyz);
	float curlInfluence = smoothstep(5.0, 30.0, dist) + smoothstep(5, 1, length(p.vel.xyz) * kExplosionDrag * dt);
	p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * curlInfluence * 15.0 * dt;

	float normLife = clamp(p.pos.w / kExplosionLifetime, 0.0, 1.0);
	p.vel.w = (1.0 - (1.0 - normLife) * (1.0 - normLife)) * 60.0;
	p.color = vec4(1.0, 0.9, 0.5, normLife);
	p.origin.w = 5.0 * normLife;
}

void updateFire(inout Particle p, float dt, float time) {
	p.vel.y += (kFireSpeed / p.pos.w) * dt;
	p.vel.x += (rand(p.pos.xy + time) - 0.5) * kFireSpread * dt * 0.25;
	p.vel.z += (rand(p.pos.yz + time) - 0.5) * kFireSpread * dt * 0.25;

	float normLife = clamp(p.pos.w / kFireLifetime, 0.0, 1.0);
	p.vel.w = smoothstep(2.0 * (1.0 - normLife), normLife, normLife / 2.5) * 25.0;
	p.color = vec4(1.0, 0.6, 0.2, normLife);
	p.origin.w = 2.0 * normLife;
}

void updateSparks(inout Particle p, float dt) {
	p.vel.xyz -= p.vel.xyz * kSparksDrag * dt;
	p.vel.y -= kSparksGravity * dt;

	vec3 hot_color = vec3(1.0, 1.0, 1.0);
	vec3 mid_color = vec3(1.0, 0.8, 0.3);
	p.color.rgb = mix(mid_color, hot_color, smoothstep(0.0, 0.5, p.pos.w));
	float pop = sin(p.pos.w * 600.0);
	p.color.rgb *= (pop > 0.0) ? 3.0 : 0.3;
	p.color.a = smoothstep(0.0, 0.1, p.pos.w);

	p.vel.w = 4.0 + p.pos.w * 20.0;
	p.origin.w = 1.0 * p.color.a;
}

void updateGlitter(inout Particle p, float dt, float time, sampler3D curlTexture) {
	p.vel.xyz -= p.vel.xyz * kGlitterDrag * dt;
	float dist = distance(p.pos.xyz, p.origin.xyz);
	float curlInfluence = smoothstep(5.0, 30.0, dist) + smoothstep(5, 1, length(p.vel.xyz) * kGlitterDrag * dt);
	p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * curlInfluence * 5.0 * dt;
	p.vel.y -= kGlitterGravity * dt;

	float hue = time * 2.0 + p.pos.w * 1.5 + float(gl_GlobalInvocationID.x) * 0.1;
	p.color.rgb = 0.6 + 0.4 * cos(hue + vec3(0, 2, 4));
	float twinkle = sin(time * 15.0 + p.pos.w * 5.0) * 0.5 + 0.5;
	p.color.rgb *= 0.6 + 0.4 * twinkle;
	p.color.rgb += vec3(pow(twinkle, 10.0) * 2.0);
	p.color.a = clamp(p.pos.w, 0.0, 1.0);

	p.vel.w = 6.0;
	p.origin.w = 0.5 * p.color.a;
}

void updateBubbles(inout Particle p, float dt, float time, sampler3D curlTexture) {
	float curlInfluence = 0.5;
	p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * curlInfluence * dt;
	p.vel.y += 0.4 * dt;
	p.vel.xyz *= 0.97;
	p.color = vec4(1.0, 1.0, 1.0, 0.6 * smoothstep(0.0, 0.5, p.pos.w));
	p.vel.w = 15.0;
	p.origin.w = 0.0; // Non-emissive
}

void updateFireflies(inout Particle p, float dt, float time, sampler3D curlTexture) {
	float curlInfluence = 0.8;
	p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * curlInfluence * dt;
	p.vel.y += 0.15 * dt;
	p.vel.xyz *= 0.99;

	vec3 firefly_base = vec3(0.7, 0.9, 0.1);
	float twinkle = sin(time * 6.0 + float(gl_GlobalInvocationID.x)) * 0.5 + 0.5;
	p.color.rgb = firefly_base * (2.0 + twinkle * 8.0);
	p.color.a = (0.4 + twinkle * 0.6) * smoothstep(0.0, 0.5, p.pos.w);
	p.vel.w = 15.0;
	p.origin.w = 0.2 * p.color.a;
}

void updateCinder(inout Particle p, float dt, float time, sampler3D curlTexture) {
	float ageFactor = clamp(1.0 - (p.pos.w / kCinderLifetime), 0.0, 1.0);
	float curlInfluence = ageFactor * kCinderDriftIntensity;
	p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * curlInfluence * dt;
	p.vel.y += kCinderBuoyancy * ageFactor * dt;
	p.vel.xyz *= 0.98;

	float cNoise = snoise3d(p.pos.xyz * 15.0 + time * 0.2);
	p.color.rgb = mix(vec3(0.1), vec3(0.25), cNoise * 0.5 + 0.5);
	float highlights = smoothstep(0.4, 0.9, snoise3d(p.pos.xyz * 40.0 + time));
	p.color.rgb = mix(p.color.rgb, vec3(2.5, 0.8, 0.2), highlights);
	p.color.a = smoothstep(0.0, 0.5, p.pos.w);
	p.vel.w = 12.0;
	p.origin.w = 0.5 * p.color.a;
}

void updateRain(inout Particle p, float dt, float time) {
	p.vel.y -= 50.0 * dt;
	vec3 wind = getWindAtPosition(p.pos.xyz);
	p.vel.xyz += wind * 5.0 * dt;
	p.vel.xyz *= 0.99;
	p.color = vec4(0.7, 0.8, 1.0, 1.0) * 2.0;
	p.color.a *= smoothstep(0.0, 0.1, p.pos.w);
	p.vel.w = 100.0;
	p.origin.w = 0.0;
}

void updateSnow(inout Particle p, float dt, float time) {
	p.vel.y -= 2.0 * dt;
	vec3 wind = getWindAtPosition(p.pos.xyz);
	p.vel.xyz += wind * 10.0 * dt;
	p.vel.x += sin(time * 5.0 + float(gl_GlobalInvocationID.x)) * 0.5 * dt;
	p.vel.z += cos(time * 4.0 + float(gl_GlobalInvocationID.x)) * 0.5 * dt;
	p.vel.xyz *= 0.95;
	p.color = vec4(1.0, 1.0, 1.0, 1.0) * 1.5;
	p.color.a *= smoothstep(0.0, 0.1, p.pos.w);
	p.vel.w = 80.0;
	p.origin.w = 0.0;
}

void updateLeaf(inout Particle p, float dt, float time, sampler3D curlTexture) {
	float curlInfluence = 1.2;
	vec3 wind = getWindAtPosition(p.pos.xyz);
	p.vel.xyz += wind * 2.0 * dt;

	p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * curlInfluence * dt;
	p.vel.y -= 0.1 * dt;
	p.vel.xyz *= pow(0.98, dt / 0.016);

	vec3 leaf_green = vec3(0.2, 0.4, 0.1);
	vec3 leaf_brown = vec3(0.4, 0.3, 0.15);
	p.color.rgb = mix(leaf_green, leaf_brown, sin(p.pos.x * 0.1 + p.pos.z * 0.1) * 0.5 + 0.5);
	float flutter = sin(time * 5.0 + p.pos.x + p.pos.y) * 0.3 + 0.7;
	p.color.rgb *= flutter;
	p.color.rgb += vec3(0.1) * pow(flutter, 10.0);
	p.color.a = smoothstep(0.0, 0.5, p.pos.w) * 0.9;
	p.vel.w = 15.0;
	p.origin.w = 0.0;
}

void updatePetal(inout Particle p, float dt, float time, sampler3D curlTexture) {
	float curlInfluence = 1.2;
	vec3 wind = getWindAtPosition(p.pos.xyz);
	p.vel.xyz += wind * 2.0 * dt;

	p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * curlInfluence * dt;
	p.vel.y -= 0.1 * dt;
	p.vel.xyz *= pow(0.98, dt / 0.016);

	p.color.rgb = vec3(1.0, 0.5, 0.8);
	float flutter = sin(time * 8.0 + p.pos.x * 2.0) * 0.4 + 0.6;
	p.color.rgb *= flutter;
	p.color.a = smoothstep(0.0, 0.5, p.pos.w) * 0.95;
	p.vel.w = 15.0;
	p.origin.w = 0.0;
}

void updateBirds(
	inout Particle p,
	float          dt,
	float          time,
	float          cellSize,
	uint           gridSize,
	sampler3D      curlTexture,
	int            num_chunks,
	sampler2DArray heightmapArray
) {
	vec3  separation = vec3(0.0);
	vec3  alignment = vec3(0.0);
	vec3  cohesion = vec3(0.0);
	int   neighborCount = 0;
	float perceptionRadius = 8.0;

	for (int x = -2; x <= 2; x++) {
		for (int y = -2; y <= 2; y++) {
			for (int z = -2; z <= 2; z++) {
				uint cellIdx = get_cell_idx(p.pos.xyz + vec3(x, y, z) * cellSize, cellSize, gridSize);
				int  otherIdx = grid_heads[cellIdx];
				int  safety = 0;
				while (otherIdx != -1 && safety < 50) {
					if (otherIdx != int(gl_GlobalInvocationID.x)) {
						Particle otherP = particles[otherIdx];
						if (otherP.style == STYLE_BIRDS) {
							float dist = distance(p.pos.xyz, otherP.pos.xyz);
							if (dist > 0.0 && dist < perceptionRadius) {
								separation += (p.pos.xyz - otherP.pos.xyz) / (dist * dist + 0.01);
								alignment += otherP.vel.xyz;
								cohesion += otherP.pos.xyz;
								neighborCount++;
							}
						}
					}
					otherIdx = grid_next[otherIdx];
					safety++;
				}
			}
		}
	}

	if (neighborCount > 0) {
		alignment /= float(neighborCount);
		cohesion = (cohesion / float(neighborCount)) - p.pos.xyz;
		p.vel.xyz += alignment * 0.4 * dt;
		p.vel.xyz += cohesion * 0.1 * dt;
		p.vel.xyz += separation * 2.5 * dt;
	}

	float terrainHeight = 0.0;
	bool  foundTerrain = false;
	for (int i = 0; i < num_chunks; i++) {
		ChunkInfo chunk = chunks[i];
		if (p.pos.x >= chunk.worldOffset.x && p.pos.x < chunk.worldOffset.x + chunk.size &&
		    p.pos.z >= chunk.worldOffset.y && p.pos.z < chunk.worldOffset.y + chunk.size) {
			vec2 uv = (p.pos.xz - chunk.worldOffset) / chunk.size;
			terrainHeight = texture(heightmapArray, vec3(uv, chunk.slice)).r;
			foundTerrain = true;
			break;
		}
	}

	if (p.pos.w < 2.0 && foundTerrain) {
		vec3  target = vec3(p.pos.x, terrainHeight - 0.2, p.pos.z);
		vec3  toTarget = target - p.pos.xyz;
		p.vel.xyz += normalize(toTarget + vec3(0, -0.1, 0)) * 5.0 * dt;
	} else {
		p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * 1.5 * dt;
		if (foundTerrain) {
			if (p.pos.y < terrainHeight + 2.0)
				p.vel.y += 2.5 * dt;
			if (p.pos.y > terrainHeight + 15.0)
				p.vel.y -= 1.5 * dt;
		}
	}

	p.vel.xyz *= pow(0.97, dt / 0.016);
	p.color = vec4(0.05, 0.05, 0.05, smoothstep(0.0, 1.0, p.pos.w));
	if (p.pos.w < 1.0 && foundTerrain && p.pos.y < terrainHeight + 0.5) {
		p.color.a *= smoothstep(0.0, 1.0, p.pos.w);
	}
	p.phase += dt * length(p.vel.xyz);
}

void updateAmbientBubble(inout Particle p, float dt, float time, sampler3D curlTexture) {
	float curlInfluence = 0.5;
	p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * curlInfluence * dt;
	p.vel.y += 0.4 * dt;
	p.vel.xyz *= pow(0.97, dt / 0.016);
	p.color = vec4(1.0, 1.0, 1.0, 0.6 * smoothstep(0.0, 0.5, p.pos.w));
	p.vel.w = 15.0;
	p.origin.w = 0.0;
}

void updateAmbientSnowflake(inout Particle p, float dt, float time, sampler3D curlTexture) {
	float curlInfluence = 0.3;
	p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * curlInfluence * dt;
	p.vel.y -= 0.5 * dt;
	p.vel.xyz *= pow(0.99, dt / 0.016);
	p.color = vec4(0.9, 0.95, 1.0, 0.8 * smoothstep(0.0, 0.5, p.pos.w)) * (1.2 + 0.3 * sin(time * 2.0 + p.pos.x));
	p.vel.w = 15.0;
	p.origin.w = 0.0;
}

void updateAmbientFairy(
	inout Particle p,
	float          dt,
	float          time,
	float          cellSize,
	uint           gridSize,
	sampler3D      curlTexture
) {
	// Sync twinkle state using counter and phase (like fireflies)
	p.counter += dt;
	float cycle_time = p.phase + 0.75;
	if (p.counter > cycle_time) {
		p.counter = fract(p.counter / cycle_time) * cycle_time;
	}

	// Simple repulsion/sync from other particles
	if (p.counter > 0.75) {
		for (int x = -1; x <= 1; x++) {
			for (int y = -1; y <= 1; y++) {
				for (int z = -1; z <= 1; z++) {
					uint cellIdx = get_cell_idx(p.pos.xyz + vec3(x, y, z) * cellSize, cellSize, gridSize);
					int  otherIdx = grid_heads[cellIdx];
					int  safety = 0;
					while (otherIdx != -1 && safety < 100) {
						if (otherIdx != int(gl_GlobalInvocationID.x)) {
							Particle otherP = particles[otherIdx];
							if (otherP.counter < 0.6) {
								float distSq = pow(distance(otherP.pos.xyz, p.pos.xyz), 2.0);
								float pulse_strength = 0.15;
								p.counter += (pulse_strength / max(distSq, 0.01)) * dt;
							}
						}
						otherIdx = grid_next[otherIdx];
						safety++;
					}
				}
			}
		}
	}

	float curlInfluence = 1.2;
	p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * curlInfluence * dt;
	p.vel.y += 0.25 * dt; // Buoyancy
	p.vel.xyz *= pow(0.98, dt / 0.016);

	// Rendering params (colors/glow handled in frag)
	float twinkle = pow(smoothstep(0.0, 0.3, p.counter) * (1.0 - smoothstep(0.4, 0.6, p.counter)), 2) * step(p.counter, 0.6);
	p.color.a = (0.3 + twinkle * 0.7) * smoothstep(0.0, 0.5, p.pos.w);
	p.vel.w = 20.0; // Size
	p.origin.w = 0.4 * p.color.a;
}

void updateAmbientFirefly(
	inout Particle p,
	float          dt,
	float          time,
	float          cellSize,
	uint           gridSize,
	sampler3D      curlTexture
) {
	// Sync twinkle state using counter and phase
	p.counter += dt;
	float cycle_time = p.phase + 0.75; // Period + refractory period
	if (p.counter > cycle_time) {
		p.counter = fract(p.counter / cycle_time) * cycle_time;
	}

	// Simple repulsion from other particles using the spatial grid
	if (p.counter > 0.75) { // Only check for sync during "flash ready" phase
		for (int x = -1; x <= 1; x++) {
			for (int y = -1; y <= 1; y++) {
				for (int z = -1; z <= 1; z++) {
					uint cellIdx = get_cell_idx(p.pos.xyz + vec3(x, y, z) * cellSize, cellSize, gridSize);
					int  otherIdx = grid_heads[cellIdx];
					int  safety = 0;
					while (otherIdx != -1 && safety < 100) {
						if (otherIdx != int(gl_GlobalInvocationID.x)) {
							Particle otherP = particles[otherIdx];
							// If neighbor is flashing, speed up our own counter to sync
							if (otherP.counter < 0.6) {
								float distSq = pow(distance(otherP.pos.xyz, p.pos.xyz), 2.0);
								float pulse_strength = 0.15;
								p.counter += (pulse_strength / max(distSq, 0.01)) * dt;
							}
						}
						otherIdx = grid_next[otherIdx];
						safety++;
					}
				}
			}
		}
	}

	float curlInfluence = 0.8;
	p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * curlInfluence * dt;
	p.vel.y += 0.15 * dt;
	p.vel.xyz *= pow(0.99, dt / 0.016);

	vec3 firefly_base = vec3(0.7, 0.9, 0.1);
	float twinkle = pow(smoothstep(0.0, 0.3, p.counter) * (1.0 - smoothstep(0.4, 0.6, p.counter)), 2) * step(p.counter, 0.6);
	p.color.rgb = firefly_base * (2.0 + twinkle * 8.0);
	p.color.a = 0.00 + step(p.counter, 0.6) * (0.4 + twinkle * 0.6) * smoothstep(0.0, 0.5, p.pos.w);
	p.vel.w = 15.0;
	p.origin.w = 0.5 * p.color.a;
}

void updateAmbientParticle(
	inout Particle p,
	float          dt,
	float          time,
	vec3           viewPos,
	vec3           viewDir,
	float          cellSize,
	uint           gridSize,
	sampler3D      curlTexture,
	int            num_chunks,
	sampler2DArray heightmapArray
) {
	float maxSpeed = 2.0;

	if (p.style == STYLE_LEAF) {
		updateLeaf(p, dt, time, curlTexture);
	} else if (p.style == STYLE_PETAL) {
		updatePetal(p, dt, time, curlTexture);
	} else if (p.style == STYLE_BUBBLES) {
		updateAmbientBubble(p, dt, time, curlTexture);
		maxSpeed = 1.5;
	} else if (p.style == STYLE_SNOW) {
		updateAmbientSnowflake(p, dt, time, curlTexture);
		maxSpeed = 1.2;
	} else if (p.style == STYLE_FIREFLIES) {
		updateAmbientFirefly(p, dt, time, cellSize, gridSize, curlTexture);
	} else if (p.style == STYLE_FAIRY) {
		updateAmbientFairy(p, dt, time, cellSize, gridSize, curlTexture);
	} else if (p.style == STYLE_BIRDS) {
		updateBirds(p, dt, time, cellSize, gridSize, curlTexture, num_chunks, heightmapArray);
		maxSpeed = 6.0;
	} else {
		p.vel.xyz += curlNoise(p.pos.xyz, time, curlTexture) * dt;
		p.vel.xyz *= pow(0.99, dt / 0.016);
		p.vel.w = 15.0;
		p.color.a = smoothstep(0.0, 0.5, p.pos.w);
		p.origin.w = 0.0;
	}

	applyAmbientAvoidance(p, dt, time, viewPos, viewDir, curlTexture);

	if (length(p.vel.xyz) > maxSpeed) {
		p.vel.xyz = normalize(p.vel.xyz) * maxSpeed;
	}

	p.pos.xyz += p.vel.xyz * dt;
	handleTerrainCollision(p, num_chunks, heightmapArray);
}

void updatePrecipitationBehavior(
	inout Particle p,
	float          dt,
	float          time,
	int            num_chunks,
	sampler2DArray heightmapArray
) {
	if (p.style == STYLE_RAIN) {
		updateRain(p, dt, time);
	} else if (p.style == STYLE_SNOW) {
		updateSnow(p, dt, time);
	}

	p.pos.xyz += p.vel.xyz * dt;
	if(handleTerrainCollision(p, num_chunks, heightmapArray)) {
		p.pos.w = 0.0;
	}
}

void updateFireBehavior(
	inout Particle p,
	float          dt,
	float          time,
	sampler3D      curlTexture,
	int            num_chunks,
	sampler2DArray heightmapArray
) {
	float maxSpeed = 10.0;
	if (p.style == STYLE_ROCKET_TRAIL) {
		updateRocketTrail(p, dt);
		maxSpeed = kExhaustSpeed;
	} else if (p.style == STYLE_EXPLOSION) {
		updateExplosion(p, dt, time, curlTexture);
		maxSpeed = kExplosionSpeed;
	} else if (p.style == STYLE_FIRE) {
		updateFire(p, dt, time);
	} else if (p.style == STYLE_SPARKS) {
		updateSparks(p, dt);
		maxSpeed = kSparksSpeed;
	} else if (p.style == STYLE_GLITTER) {
		updateGlitter(p, dt, time, curlTexture);
		maxSpeed = kGlitterSpeed;
	} else if (p.style == STYLE_BUBBLES) {
		updateBubbles(p, dt, time, curlTexture);
		maxSpeed = 1.5;
	} else if (p.style == STYLE_FIREFLIES) {
		updateFireflies(p, dt, time, curlTexture);
		maxSpeed = 2.0;
	} else if (p.style == STYLE_FAIRY) {
		// Reuse ambient fairy logic for emitters too
		updateAmbientFairy(p, dt, time, 1.0, 1, curlTexture);
		maxSpeed = 3.0;
	} else if (p.style == STYLE_CINDER) {
		updateCinder(p, dt, time, curlTexture);
		maxSpeed = 5.0;
	} else if (p.style == STYLE_RAIN) {
		updateRain(p, dt, time);
		maxSpeed = 40.0;
	} else if (p.style == STYLE_SNOW) {
		updateSnow(p, dt, time);
		maxSpeed = 5.0;
	}

	if (p.style != STYLE_SPARKS && p.style != STYLE_GLITTER && p.style != STYLE_AMBIENT && p.style != STYLE_BUBBLES && p.style != STYLE_FIREFLIES && p.style != STYLE_DEBUG && p.style != STYLE_CINDER &&
	    p.style != STYLE_FIRE && p.style != STYLE_RAIN && p.style != STYLE_SNOW) {
		p.vel.y -= 0.05 * dt;
	}

	if (p.style != STYLE_AMBIENT && p.style != STYLE_BUBBLES && p.style != STYLE_FIREFLIES && p.style != STYLE_DEBUG && p.style != STYLE_CINDER && p.style != STYLE_FIRE && p.style != STYLE_RAIN && p.style != STYLE_SNOW) {
		p.vel.xyz += vec3(mix(curlNoise(p.pos.xyz, time, curlTexture) * 3, vec3(0, 0, 0), length(p.vel) / maxSpeed)) *
			dt;
	}

	p.pos.xyz += p.vel.xyz * dt;
	handleTerrainCollision(p, num_chunks, heightmapArray);
}

void updateBehavior(
	inout Particle p,
	float          dt,
	float          time,
	vec3           viewPos,
	vec3           viewDir,
	float          cellSize,
	uint           gridSize,
	sampler3D      curlTexture,
	int            num_chunks,
	sampler2DArray heightmapArray
) {
	if (p.emitter_id == -1) {
		updateAmbientParticle(
			p,
			dt,
			time,
			viewPos,
			viewDir,
			cellSize,
			gridSize,
			curlTexture,
			num_chunks,
			heightmapArray
		);
	} else if (p.emitter_id == -2) {
		updatePrecipitationBehavior(p, dt, time, num_chunks, heightmapArray);
	} else {
		updateFireBehavior(p, dt, time, curlTexture, num_chunks, heightmapArray);
	}
}

#endif
#endif // GSHADERS_PARTICLE_BEHAVIOR_GLSL
