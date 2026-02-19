#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;

out vec3  vs_color;
out float vs_progress;
out vec3  vs_normal;
out vec3  vs_frag_pos;
flat out int vs_trail_idx;

uniform mat4  view;
uniform mat4  projection;
uniform vec4  clipPlane;

struct TrailParams {
	vec4  min_bound; // w = base_thickness
	vec4  max_bound; // w = roughness
	uvec4 config1;   // x=vertex_offset, y=max_vertices, z=head, w=tail
	uvec4 config2;   // x=vertex_count, y=is_full, z=iridescent, w=rocket_trail
	vec4  config3;   // x=use_pbr, y=metallic, zw=padding
};

layout(std430, binding = 7) readonly buffer TrailParamsBuffer {
	TrailParams trails[];
};

#include "helpers/lighting.glsl"
#include "helpers/noise.glsl"

void main() {
	int         trail_idx = gl_BaseInstance;
	TrailParams trail = trails[trail_idx];

	float base_thickness = trail.min_bound.w;
	bool  useRocketTrail = trail.config2.w != 0u;
	float vertex_offset = float(trail.config1.x);
	float head = float(trail.config1.z);
	float vertex_count = float(trail.config2.x);

	float trailHead = vertex_offset + head;
	float trailSize = vertex_count;

	// Convert vertex ID to a sequential Ring Index (0, 1, 2... N)
	float vertsPerStep = 18.0;
	float segmentIndex = floor(float(gl_VertexID) / vertsPerStep);
	float isBottomRing = mod(float(gl_VertexID) + 1.0, 2.0);
	float logicalRingIndex = segmentIndex + isBottomRing;

	// Convert raw "vertex counts" into "ring counts" so units match
	float headRingIndex = floor(trailHead / vertsPerStep);
	float totalRings = floor(trailSize / vertsPerStep);

	float ringDist = logicalRingIndex - headRingIndex;

	if (ringDist < 0.0) {
		ringDist += totalRings;
	}

	float Progress = ringDist / (totalRings > 0.0 ? totalRings : 1.0);
	Progress = clamp(Progress, 0.0, 1.0);

	// Calculate tapering scale based on progress
	// Progress 0 = oldest (tail), Progress 1 = newest (head)
	float taper_scale = 0.2 + 0.8 * Progress;

	// Calculate the displacement required to scale the tube's radius
	vec3 offset = aNormal * base_thickness * (taper_scale - 1.0);

	if (useRocketTrail) {
		offset = vec3(0);
		// Billowing smoke effect
		float noise_freq = 3.0;
		float noise_strength = 2.6 * (1.0 - Progress);
		float noise = snoise(
			vec2(mix(2.0 / (Progress + 0.01), 2.0 * Progress, Progress) * noise_freq, mix(time / 3.5, time / 2.0, Progress))
		);
		offset += aNormal * noise * noise_strength * base_thickness;

		// Make the trail expand as it gets older (lower progress)
		offset += aNormal * base_thickness * mix(5.0, 0.0, Progress) * mix(4.0 * (noise * 0.5 + 0.5), 4.0, Progress);
	}

	vec3 final_pos = aPos + offset;

	vs_color = aColor;
	vs_progress = Progress;
	vs_trail_idx = trail_idx;
	vs_normal = aNormal;
	vs_frag_pos = final_pos;

	gl_ClipDistance[0] = dot(vec4(final_pos, 1.0), clipPlane);
	gl_Position = projection * view * vec4(final_pos, 1.0);
}
