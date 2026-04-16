#ifndef PARTICLE_TYPES_GLSL
#define PARTICLE_TYPES_GLSL

// Must match the C++ struct layout in fire_effect.h
struct Particle {
	vec4  pos; // Position (w is lifetime)
	vec4  vel; // Velocity (w is unused)
	vec3  epicenter;
	int   style;
	int   emitter_index;
	int   emitter_id;
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
layout(std430, binding = [[PARTICLE_BUFFER_BINDING]]) buffer ParticleBuffer {
	Particle particles[];
};

layout(std430, binding = [[EMITTER_BUFFER_BINDING]]) buffer EmitterBuffer {
	Emitter emitters[];
};

layout(std430, binding = [[VISIBLE_PARTICLE_INDICES_BINDING]]) writeonly buffer VisibleIndicesBuffer {
	uint visible_indices[];
};

layout(std430, binding = [[LIVE_PARTICLE_INDICES_BINDING]]) buffer LiveIndicesBuffer {
	uint live_indices[];
};

layout(std430, binding = [[PARTICLE_GRID_HEADS_BINDING]]) buffer ParticleGridHeads {
	int grid_heads[];
};

layout(std430, binding = [[PARTICLE_GRID_NEXT_BINDING]]) buffer ParticleGridNext {
	int grid_next[];
};
#else
layout(std430, binding = [[PARTICLE_BUFFER_BINDING]]) readonly buffer ParticleBuffer {
	Particle particles[];
};

layout(std430, binding = [[EMITTER_BUFFER_BINDING]]) readonly buffer EmitterBuffer {
	Emitter emitters[];
};

layout(std430, binding = [[VISIBLE_PARTICLE_INDICES_BINDING]]) readonly buffer VisibleIndicesBuffer {
	uint visible_indices[];
};

layout(std430, binding = [[LIVE_PARTICLE_INDICES_BINDING]]) readonly buffer LiveIndicesBuffer {
	uint live_indices[];
};

layout(std430, binding = [[PARTICLE_GRID_HEADS_BINDING]]) readonly buffer ParticleGridHeads {
	int grid_heads[];
};

layout(std430, binding = [[PARTICLE_GRID_NEXT_BINDING]]) readonly buffer ParticleGridNext {
	int grid_next[];
};
#endif

layout(std430, binding = [[INDIRECTION_BUFFER_BINDING]]) buffer IndirectionBuffer {
	int particle_to_emitter_map[];
};

layout(std430, binding = [[TERRAIN_CHUNK_INFO_BINDING]]) buffer ChunkInfoBuffer {
	ChunkInfo chunks[];
};

layout(std430, binding = [[SLICE_DATA_BINDING]]) buffer SliceDataBuffer {
	vec4 slice_data[];
};

layout(std430, binding = [[PARTICLE_DRAW_COMMAND_BINDING]]) buffer DrawCommandBuffer {
	DrawArraysIndirectCommand draw_command;
};

layout(std430, binding = [[BEHAVIOR_DRAW_COMMAND_BINDING]]) buffer BehaviorCommandBuffer {
	DispatchIndirectCommand behavior_command;
};

#endif
