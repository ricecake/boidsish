#version 460 core
layout(location = 0) in vec4 aPos;    // xyz: pos, w: progress
layout(location = 1) in vec4 aNormal; // xyz: normal, w: ?
layout(location = 2) in vec4 aColor;  // rgb: color, w: ?

out vec3  vs_color;
out float vs_progress;
out vec3  vs_normal;
out vec3  vs_frag_pos;
flat out int vUniformIndex;

#include "common_uniforms.glsl"

layout(std430, binding = 2) buffer UniformsSSBO {
	CommonUniforms uniforms_data[];
};

uniform bool uUseMDI = false;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec4 clipPlane;

#include "helpers/lighting.glsl"
#include "helpers/noise.glsl"
#include "temporal_data.glsl"

out vec4 CurPosition;
out vec4 PrevPosition;

void main() {
	vUniformIndex = uUseMDI ? gl_DrawID : -1;
	bool use_ssbo = uUseMDI && vUniformIndex >= 0;

	mat4  current_model = use_ssbo ? uniforms_data[vUniformIndex].model : model;
	float base_thickness = use_ssbo ? uniforms_data[vUniformIndex].ao : 0.06;
	int   flags = use_ssbo ? uniforms_data[vUniformIndex].is_line : 0;
	bool  useRocketTrail = (flags & 2) != 0;

	float Progress = aPos.w;

	vec3 offset = vec3(0);
	if (useRocketTrail) {
		// Billowing smoke effect
		float noise_freq = 3.0;
		float noise_strength = 2.6 * (1.0 - Progress);
		float noise = snoise(
			vec2(mix(2 / (Progress + 0.01), 2 * Progress, Progress) * noise_freq, mix(time / 3.5, time / 2, Progress))
		);

		offset += aNormal.xyz * noise * noise_strength * base_thickness;

		// Make the trail expand as it gets older (lower progress)
		offset += aNormal.xyz * base_thickness * mix(5, 0, Progress) * mix(4 * (noise * 0.5 + 0.5), 4, Progress);
	}

	vec3 final_pos = aPos.xyz + offset;

	vs_color = aColor.rgb;
	vs_progress = Progress;
	vs_normal = mat3(transpose(inverse(current_model))) * aNormal.xyz;
	vs_frag_pos = vec3(current_model * vec4(final_pos, 1.0));

	vec4 world_pos = current_model * vec4(final_pos, 1.0);
	gl_ClipDistance[0] = dot(world_pos, clipPlane);
	gl_Position = projection * view * world_pos;

	CurPosition = gl_Position;
	PrevPosition = prevViewProjection * world_pos;
}
