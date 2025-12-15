#version 420 core
out vec4 FragColor;

in vec3 vs_color;
in vec3 vs_normal;
in vec3 vs_frag_pos;
in vec3 barycentric;

#include "artistic_effects.frag"

layout(std140, binding = 1) uniform VisualEffects {
    bool  ripple_enabled;
    float ripple_strength;
    bool  color_shift_enabled;
    float color_shift_strength;
};

layout(std140) uniform Lighting {
	vec3  lightPos;
	vec3  viewPos;
	vec3  lightColor;
	float time;
};

void main() {
	// Ambient
	float ambient_strength = 0.2;
	vec3  ambient = ambient_strength * lightColor;

	// Diffuse
	vec3  norm = normalize(vs_normal);
	vec3  light_dir = normalize(lightPos - vs_frag_pos);
	float diff = max(dot(norm, light_dir), 0.0);
	vec3  diffuse = diff * lightColor;

	// Specular
	float specular_strength = 0.5;
	vec3  view_dir = normalize(viewPos - vs_frag_pos);
	vec3  reflect_dir = reflect(-light_dir, norm);
	float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32);
	vec3  specular = specular_strength * spec * lightColor;

	vec3 result = (ambient + diffuse) * vs_color + specular;
	result = applyArtisticEffects(result, vs_frag_pos, barycentric, time);
	FragColor = vec4(result, 1.0);
}
