#version 460 core
#ifndef GSHADERS_FIRE_VERT
#define GSHADERS_FIRE_VERT
#extension GL_ARB_shader_draw_parameters : enable

//START shaders/frustum.glsl
#ifndef GSHADERS_FRUSTUM_GLSL
#define GSHADERS_FRUSTUM_GLSL
#ifndef FRUSTUM_GLSL
#define FRUSTUM_GLSL

//START shaders/types/frustum.glsl
#ifndef GSHADERS_TYPES_FRUSTUM_GLSL
#define GSHADERS_TYPES_FRUSTUM_GLSL
#ifndef FRUSTUM_TYPES_GLSL
#define FRUSTUM_TYPES_GLSL

// Frustum UBO for GPU-side frustum culling (binding point 3)
// Each plane is stored as vec4: xyz = normal, w = distance
layout(std140, binding = 3) uniform FrustumData {
	vec4  frustumPlanes[6]; // Left, Right, Bottom, Top, Near, Far
	vec3  frustumCameraPos; // Camera position for distance-based LOD
	float frustumPadding;   // Padding for std140 alignment
};

#endif
#endif // GSHADERS_TYPES_FRUSTUM_GLSL
//END shaders/types/frustum.glsl (returning to shaders/frustum.glsl)

// Check if a point is inside the frustum
// Returns true if inside, false if outside
bool isPointInFrustum(vec3 point) {
	for (int i = 0; i < 6; i++) {
		if (dot(frustumPlanes[i].xyz, point) + frustumPlanes[i].w < 0.0) {
			return false;
		}
	}
	return true;
}

// Check if a sphere is inside or intersecting the frustum
// Returns true if inside or intersecting, false if completely outside
bool isSphereInFrustum(vec3 center, float radius) {
	for (int i = 0; i < 6; i++) {
		if (dot(frustumPlanes[i].xyz, center) + frustumPlanes[i].w < -radius) {
			return false;
		}
	}
	return true;
}

// Check if an AABB is inside or intersecting the frustum
// Returns true if inside or intersecting, false if completely outside
bool isAABBInFrustum(vec3 minCorner, vec3 maxCorner) {
	for (int i = 0; i < 6; i++) {
		vec3 normal = frustumPlanes[i].xyz;
		// Find the positive vertex (corner most in direction of plane normal)
		vec3 pVertex = vec3(
			normal.x >= 0.0 ? maxCorner.x : minCorner.x,
			normal.y >= 0.0 ? maxCorner.y : minCorner.y,
			normal.z >= 0.0 ? maxCorner.z : minCorner.z
		);
		if (dot(normal, pVertex) + frustumPlanes[i].w < 0.0) {
			return false;
		}
	}
	return true;
}

#endif
#endif // GSHADERS_FRUSTUM_GLSL
//END shaders/frustum.glsl (returning to shaders/fire.vert)
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
//END shaders/lighting.glsl (returning to shaders/fire.vert)
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
//END shaders/particle_types.glsl (returning to shaders/fire.vert)

uniform mat4  u_view;
uniform mat4  u_projection;
uniform vec3  u_camera_pos;
uniform bool  enableFrustumCulling = false;
uniform float frustumCullRadius = 1.0;

out float         v_lifetime;
out vec4          view_pos;
out vec4          v_pos;
out vec3          v_vel;
out vec3          v_vel_view;
out vec3          v_origin;
flat out int      v_style;
flat out int      v_emitter_index;
flat out int      v_emitter_id;
flat out uint     v_particle_idx;
flat out Particle v_p;

void main() {
	uint     particle_idx = visible_indices[gl_VertexID];
	Particle p = particles[particle_idx];
	v_pos = p.pos;
	v_vel = p.vel.xyz;
	v_vel_view = (u_view * vec4(p.vel.xyz, 0.0)).xyz;
	v_origin = p.origin.xyz;
	v_style = p.style;
	v_emitter_index = p.emitter_index;
	v_emitter_id = p.emitter_id;
	v_particle_idx = particle_idx;
	v_p = p;

	{
		view_pos = u_view * vec4(p.pos.xyz, 1.0);
		gl_Position = u_projection * view_pos;
		v_lifetime = p.pos.w;

		float base_size = p.vel.w;

		if (p.style == STYLE_RAIN) {
			gl_PointSize = clamp(base_size / (-view_pos.z * 0.1), 2.0, 30.0);
		} else if (p.style == STYLE_SNOW) {
			gl_PointSize = clamp(base_size / (-view_pos.z * 0.1), 4.0, 40.0);
		} else if (p.style == STYLE_AMBIENT || p.style == STYLE_BUBBLES || p.style == STYLE_FIREFLIES || p.style == STYLE_CINDER || p.style == STYLE_LEAF || p.style == STYLE_PETAL || p.style == STYLE_BIRDS || p.style == STYLE_FAIRY) {
			gl_PointSize = base_size / (-view_pos.z * 0.05);

			float size_var = fract(sin(float(particle_idx) * 123.456) * 456.789);
			if (p.style == STYLE_BUBBLES) {
				gl_PointSize *= (0.5 + size_var * 1.5);
			} else if (p.style == STYLE_CINDER) {
				gl_PointSize *= (0.6 + size_var * 0.4) * 0.8;
			} else {
				gl_PointSize *= (0.8 + size_var * 0.4);
			}
			gl_PointSize = clamp(gl_PointSize, 2.0, 40.0);
		} else {
			gl_PointSize = base_size;
		}
	}
}
#endif // GSHADERS_FIRE_VERT
