#ifndef HELPERS_LIGHTING_GLSL
#define HELPERS_LIGHTING_GLSL

#include "../lighting.glsl"

vec3 apply_lighting(vec3 frag_pos, vec3 normal, vec3 albedo, float ambient_strength, float specular_strength) {
	vec3 ambient = ambient_strength * albedo;
	vec3 result = ambient;

	for (int i = 0; i < num_lights; ++i) {
		// Diffuse
		vec3  light_dir = normalize(lights[i].position - frag_pos);
		float diff = max(dot(normal, light_dir), 0.0);
		vec3  diffuse = lights[i].color * diff * albedo;

		// Specular
		vec3  view_dir = normalize(viewPos - frag_pos);
		vec3  reflect_dir = reflect(-light_dir, normal);
		float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32);
		vec3  specular = lights[i].color * spec * specular_strength;

		result += (diffuse + specular) * lights[i].intensity;
	}

	return result;
}

#endif // HELPERS_LIGHTING_GLSL
