#version 460 core

#include "common_uniforms.glsl"

layout(std430, binding = 2) buffer UniformsSSBO {
	CommonUniforms uniforms_data[];
};

uniform bool uUseMDI = false;
flat in int  vUniformIndex;

layout(location = 0) out vec4 FragColor;

in vec3 FragPos;
in mat4 vModel;

uniform vec3 u_viewPos;
uniform sampler3D u_sdf_texture;

float sampleSDF(vec3 p_model, vec3 min_bounds, vec3 max_bounds) {
	vec3 size = max_bounds - min_bounds;
	if (length(size) < 0.001)
		return 1000.0;
	vec3 uvw = (p_model - min_bounds) / size;
	if (any(lessThan(uvw, vec3(0))) || any(greaterThan(uvw, vec3(1))))
		return 1000.0;
	return texture(u_sdf_texture, uvw).r;
}

void main() {
	int  idx = vUniformIndex;
	vec3 min_bounds = (uUseMDI && idx >= 0) ? uniforms_data[idx].sdf_aabb_min.xyz : vec3(-1.0);
	vec3 max_bounds = (uUseMDI && idx >= 0) ? uniforms_data[idx].sdf_aabb_max.xyz : vec3(1.0);

	vec3 rayDir = normalize(FragPos - u_viewPos);

	mat4 invModel = inverse(vModel);
	vec3 cam_model = vec3(invModel * vec4(u_viewPos, 1.0));

	vec3 p = FragPos;
	if (all(greaterThan(cam_model, min_bounds)) && all(lessThan(cam_model, max_bounds))) {
		p = u_viewPos;
	}

	float scale = length(vec3(vModel[0]));

	float totalDist = 0.0;
	for (int i = 0; i < 256; i++) {
		vec3  p_model = vec3(invModel * vec4(p, 1.0));
		float d_model = sampleSDF(p_model, min_bounds, max_bounds);
		float d_world = d_model * scale;

		if (abs(d_world) < 0.002) {
			vec3 size = max_bounds - min_bounds;
			vec3 uvw = (p_model - min_bounds) / size;
			vec3 normal_model = texture(u_sdf_texture, uvw).gba;

			mat3 normalMatrix = transpose(mat3(invModel));
			vec3 worldNormal = normalize(normalMatrix * normal_model);

			vec3  lightDir = normalize(vec3(1.0, 1.0, 1.0));
			float diff = max(dot(worldNormal, lightDir), 0.2);
			FragColor = vec4(vec3(0.2, 0.6, 1.0) * diff, 1.0);
			return;
		}

		if (totalDist > 100.0)
			break;

		float step = max(abs(d_world), 0.005);
		p += rayDir * step;
		totalDist += step;

		vec3 next_p_model = vec3(invModel * vec4(p, 1.0));
		if (any(lessThan(next_p_model, min_bounds - 0.1)) || any(greaterThan(next_p_model, max_bounds + 0.1))) {
			break;
		}
	}
	discard;
}
