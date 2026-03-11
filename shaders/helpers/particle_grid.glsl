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

const uint  u_volume_size = [[PARTICLE_VOLUME_SIZE]];
const float u_volume_scale = [[PARTICLE_VOLUME_SCALE]];

uniform sampler3D u_particleVolume;
uniform vec3      u_particleVolumeOrigin;

#include "fast_noise.glsl"

float get_particle_density(vec3 pos, float radius) {
	// Sample from the 3D density volume for O(1) lookups
	vec3  relPos = pos - u_particleVolumeOrigin;
	vec3  volumeCoords = relPos / u_volume_scale;
	vec3  uvw = volumeCoords / float(u_volume_size);

	if (uvw.x < 0.0 || uvw.x > 1.0 || uvw.y < 0.0 || uvw.y > 1.0 || uvw.z < 0.0 || uvw.z > 1.0) {
		return 0.0;
	}

	// Leveraging hardware trilinear interpolation for smooth density fields
	return texture(u_particleVolume, uvw).r;
}

// Simple raymarching through the grid
// Accumulates density along the ray
float trace_particle_density(vec3 rayOrigin, vec3 rayDir, float maxDist, float stepSize, float radius) {
	float totalDensity = 0.0;
	float t = 0.0;

	// Jitter starting position to reduce stepping artifacts
	float jitter = fastSimplex3d(rayOrigin * 0.1 + rayDir * 0.1) * stepSize;
	t += jitter;

	// Dynamically determine step count but cap for safety
	int steps = clamp(int(maxDist / stepSize), 1, 512);

	for (int i = 0; i < steps; i++) {
		vec3 p = rayOrigin + rayDir * t;
		float d = get_particle_density(p, radius);

		// Volumetric accumulation: d is density, exp(-d) is transmission
		totalDensity += d * stepSize;

		// Early exit if the medium is already practically opaque
		if (exp(-totalDensity * 0.05) < 0.01) {
			break;
		}

		t += stepSize;
	}

	return totalDensity;
}

#endif
