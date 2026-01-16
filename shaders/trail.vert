#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;

layout(std140) uniform Lighting {
	vec3  lightPos;
	vec3  viewPos;
	vec3  lightColor;
	float time;
};

out vec3  vs_color;
out float vs_progress;
out vec3  vs_normal;
out vec3  vs_frag_pos;

uniform mat4  model;
uniform mat4  view;
uniform mat4  projection;
uniform vec4  clipPlane;
uniform float base_thickness;
uniform bool  useRocketTrail;
uniform float trailHead;
uniform float trailSize;

#include "helpers/noise.glsl"

void main() {
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

	float Progress = ringDist / totalRings;
	Progress = clamp(Progress, 0.0, 1.0);

	// Calculate tapering scale based on progress
	// Progress 0 = oldest (tail), Progress 1 = newest (head)
	// We want thick at head (newest) and thin at tail (oldest)
	float taper_scale = 0.2 + 0.8 * Progress;

	// Calculate the displacement required to scale the tube's radius
	// This moves the vertex closer to or further from the tube's spine
	vec3 offset = aNormal * base_thickness * (taper_scale - 1.0);

	if (useRocketTrail) {
		offset = vec3(0);
		// Billowing smoke effect
		float noise_freq = 3.0;
		// Noise is stronger at the tail (lower progress)
		float noise_strength = 2.6 * (1.0 - Progress);
		// float noise = snoise(vec2(mix(2/(aProgress+0.01), 2*aProgress, aProgress) * noise_freq, mix(time/3.5, time
		// * 3.5, aProgress)));
		float noise = snoise(
			vec2(mix(2 / (Progress + 0.01), 2 * Progress, Progress) * noise_freq, mix(time / 3.5, time / 2, Progress))
		);
		offset += aNormal * noise * noise_strength * base_thickness;

		// Make the trail expand as it gets older (lower progress)
		offset += aNormal * base_thickness * mix(5, 0, Progress) * mix(4 * (noise * 0.5 + 0.5), 4, Progress);
	}

	vec3 final_pos = aPos + offset;

	vs_color = aColor;
	vs_progress = Progress; // Pass progress along, might be useful later
	vs_normal = mat3(transpose(inverse(model))) * aNormal;
	vs_frag_pos = vec3(model * vec4(final_pos, 1.0));

	vec4 world_pos = model * vec4(final_pos, 1.0);
	gl_ClipDistance[0] = dot(world_pos, clipPlane);
	gl_Position = projection * view * world_pos;
}
