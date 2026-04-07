#version 430 core
#extension GL_GOOGLE_include_directive : enable

#include "common_uniforms.glsl"

layout(std430, binding = 2) buffer UniformsSSBO {
	CommonUniforms uniforms_data[];
};

uniform bool uUseMDI = false;
uniform bool uEnableColor = false;
in vec3      FragPos;
in vec3      Normal;
in vec4      vColor;
flat in int  vUniformIndex;

uniform bool  dissolve_enabled = false;
uniform vec3  dissolve_plane_normal = vec3(0, 1, 0);
uniform float dissolve_plane_dist = 0.0;

layout(location = 0) out vec4 FragColor;

void main() {
	bool  use_ssbo = uUseMDI && vUniformIndex >= 0;
	bool  c_dissolve_enabled = use_ssbo ? (uniforms_data[vUniformIndex].dissolve_enabled != 0) : dissolve_enabled;
	vec3  c_dissolve_normal = use_ssbo ? uniforms_data[vUniformIndex].dissolve_plane_normal : dissolve_plane_normal;
	float c_dissolve_dist = use_ssbo ? uniforms_data[vUniformIndex].dissolve_plane_dist : dissolve_plane_dist;

	if (c_dissolve_enabled) {
		if (dot(FragPos, c_dissolve_normal) > c_dissolve_dist) {
			discard;
		}
	}

	if (uEnableColor) {
		// Output surface color for reflected lighting
		// Simple NdotL with upward vector as proxy for ambient bounce
		float bounce = max(0.0, dot(normalize(Normal), vec3(0, 1, 0))) * 0.5 + 0.5;
		FragColor = vec4(vColor.rgb * bounce, 0.1); // Low alpha for temporal fading
	}
}
