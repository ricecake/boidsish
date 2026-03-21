#ifndef PARTICLE_TYPES_GLSL
#define PARTICLE_TYPES_GLSL

// Must match the C++ struct layout in fire_effect.h
struct Particle {
	vec4 pos; // Position (w is lifetime)
	vec4 vel; // Velocity (w is unused)
	vec3 epicenter;
	int  style;
	int  emitter_index;
	int  emitter_id;
	float extras[2];
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

#endif
