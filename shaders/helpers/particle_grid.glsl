#ifndef PARTICLE_GRID_GLSL
#define PARTICLE_GRID_GLSL

#include "spatial_hash.glsl"

struct Particle {
	vec4 pos; // Position (w is lifetime)
	vec4 vel; // Velocity (w is unused)
	vec3 epicenter;
	int  style;
	int  emitter_index;
	int  emitter_id;
	int  _padding[2];
};

layout(std430, binding = 16) readonly buffer ParticleBuffer {
	Particle particles[];
};

layout(std430, binding = 14) readonly buffer ParticleGridHeads {
	int grid_heads[];
};

layout(std430, binding = 15) readonly buffer ParticleGridNext {
	int grid_next[];
};

const uint  u_grid_size = [[PARTICLE_GRID_SIZE]];
const float u_cell_size = [[PARTICLE_GRID_CELL_SIZE]];

#include "fast_noise.glsl"

float get_particle_density(vec3 pos, float radius) {
	float density = 0.0;
	float radiusSq = radius * radius;

	// Scan neighboring cells
	for (int x = -1; x <= 1; x++) {
		for (int y = -1; y <= 1; y++) {
			for (int z = -1; z <= 1; z++) {
				uint cellIdx = get_cell_idx(pos + vec3(x, y, z) * u_cell_size, u_cell_size, u_grid_size);
				int  pIdx = grid_heads[cellIdx];

				int safety = 0;
				while (pIdx != -1 && safety < 100) {
					vec3  pPos = particles[pIdx].pos.xyz;
					float distSq = dot(pos - pPos, pos - pPos);
					if (distSq < radiusSq) {
						float dist = sqrt(distSq);
						// Quadratic falloff for smoother density
						density += 1.0 - (dist / radius);
					}
					pIdx = grid_next[pIdx];
					safety++;
				}
			}
		}
	}
	return density;
}

// Simple raymarching through the grid
// Accumulates density along the ray
float trace_particle_density(vec3 rayOrigin, vec3 rayDir, float maxDist, float stepSize, float radius) {
	float totalDensity = 0.0;
	float t = 0.0;

	// Jitter starting position to reduce stepping artifacts
	float jitter = fastSimplex3d(rayOrigin * 0.1 + rayDir * 0.1) * stepSize;
	t += jitter;

	for (int i = 0; i < 128; i++) {
		if (t > maxDist)
			break;

		vec3 p = rayOrigin + rayDir * t;
		float d = get_particle_density(p, radius);

		// Volumetric accumulation: d is density, exp(-d) is transmission
		totalDensity += d * stepSize;

		t += stepSize;
	}

	return totalDensity;
}

#endif
