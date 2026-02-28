#version 460 core
#extension GL_GOOGLE_include_directive : enable

#include "common_uniforms.glsl"

layout(std430, binding = 2) buffer UniformsSSBO {
	CommonUniforms uniforms_data[];
};

uniform bool uUseMDI = false;
in vec3      FragPos;
flat in int  vUniformIndex;

uniform bool  dissolve_enabled = false;
uniform vec3  dissolve_plane_normal = vec3(0, 1, 0);
uniform float dissolve_plane_dist = 0.0;

void main() {
	bool  use_ssbo = uUseMDI && vUniformIndex >= 0;
	bool  c_dissolve_enabled = use_ssbo ? (uniforms_data[vUniformIndex].dissolve_enabled != 0) : dissolve_enabled;
	vec3  c_dissolve_normal = use_ssbo ? uniforms_data[vUniformIndex].dissolve_plane_normal_dist.xyz : dissolve_plane_normal;
	float c_dissolve_dist = use_ssbo ? uniforms_data[vUniformIndex].dissolve_plane_normal_dist.w : dissolve_plane_dist;

	if (c_dissolve_enabled) {
		if (dot(FragPos, c_dissolve_normal) > c_dissolve_dist) {
			discard;
		}
	}
}
