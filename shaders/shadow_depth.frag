#version 430 core


#include "types/common.glsl"
#include "types/temporal_data.glsl"
#include "types/lighting.glsl"
#include "types/lighting.glsl"
#include "types/terrain.glsl"
#include "types/biomes.glsl"
#include "types/shadows.glsl"


uniform bool uUseMDI = false;
in vec3      FragPos;
flat in int  vUniformIndex;

uniform bool  dissolve_enabled = false;
uniform vec3  dissolve_plane_normal = vec3(0, 1, 0);
uniform float dissolve_plane_dist = 0.0;

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
}
