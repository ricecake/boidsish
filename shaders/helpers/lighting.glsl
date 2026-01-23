#ifndef HELPERS_LIGHTING_GLSL
#define HELPERS_LIGHTING_GLSL

#include "../lighting.glsl"

const int LIGHT_TYPE_POINT = 0;
const int LIGHT_TYPE_DIRECTIONAL = 1;
const int LIGHT_TYPE_SPOT = 2;

/**
 * Calculate shadow factor for a fragment position using a specific shadow map.
 * Returns 0.0 if fully in shadow, 1.0 if fully lit.
 * Uses PCF (Percentage Closer Filtering) for soft shadow edges.
 */
float calculateShadow(int shadow_index, vec3 frag_pos) {
	// Early out for invalid indices or when no shadow lights are active
	// This MUST return before any texture operations to avoid driver issues
	if (shadow_index < 0) {
		return 1.0; // No shadow for this light
	}
	if (numShadowLights <= 0) {
		return 1.0; // No shadow maps active at all
	}
	if (shadow_index >= MAX_SHADOW_LIGHTS || shadow_index >= numShadowLights) {
		return 1.0; // Index out of bounds
	}

	// Transform fragment position to light space
	vec4 frag_pos_light_space = lightSpaceMatrices[shadow_index] * vec4(frag_pos, 1.0);

	// Perspective divide (guard against division by zero)
	if (abs(frag_pos_light_space.w) < 0.0001) {
		return 1.0;
	}
	vec3 proj_coords = frag_pos_light_space.xyz / frag_pos_light_space.w;

	// Transform to [0,1] range for texture sampling
	proj_coords = proj_coords * 0.5 + 0.5;

	// Check if fragment is outside the shadow map frustum
	if (proj_coords.x < 0.0 || proj_coords.x > 1.0 || proj_coords.y < 0.0 || proj_coords.y > 1.0 ||
	    proj_coords.z > 1.0 || proj_coords.z < 0.0) {
		return 1.0; // Outside shadow frustum, fully lit
	}

	// Current depth from light's perspective
	float current_depth = proj_coords.z;

	// Bias to prevent shadow acne (adjust based on surface angle)
	float bias = 0.002;

	// PCF - sample multiple texels for soft shadows
	float shadow = 0.0;
	vec2  texel_size = 1.0 / vec2(textureSize(shadowMaps, 0).xy);

	// 3x3 PCF kernel
	for (int x = -1; x <= 1; ++x) {
		for (int y = -1; y <= 1; ++y) {
			vec2 offset = vec2(x, y) * texel_size;
			// sampler2DArrayShadow expects (u, v, layer, compare_value)
			vec4 shadow_coord = vec4(proj_coords.xy + offset, float(shadow_index), current_depth - bias);
			shadow += texture(shadowMaps, shadow_coord);
		}
	}
	shadow /= 9.0;

	return shadow;
}

/**
 * Apply lighting with shadow support.
 * This version checks each light for shadow casting capability.
 */
vec3 apply_lighting(vec3 frag_pos, vec3 normal, vec3 albedo, float specular_strength) {
	vec3 result = ambient_light * albedo;

	for (int i = 0; i < num_lights; ++i) {
		vec3  light_dir;
		float attenuation = 1.0;

		if (lights[i].type == LIGHT_TYPE_POINT) { // Point light
			light_dir = normalize(lights[i].position - frag_pos);
			float distance = length(lights[i].position - frag_pos);
			attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * (distance * distance));
		} else if (lights[i].type == LIGHT_TYPE_DIRECTIONAL) { // Directional light
			light_dir = normalize(-lights[i].direction);
		} else if (lights[i].type == LIGHT_TYPE_SPOT) { // Spot light
			light_dir = normalize(lights[i].position - frag_pos);
			float distance = length(lights[i].position - frag_pos);
			attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * (distance * distance));
			float theta = dot(light_dir, normalize(-lights[i].direction));
			float epsilon = lights[i].inner_cutoff - lights[i].outer_cutoff;
			float intensity = clamp((theta - lights[i].outer_cutoff) / epsilon, 0.0, 1.0);
			attenuation *= intensity;
		}

		// Calculate shadow factor for this light
		float shadow = calculateShadow(lightShadowIndices[i], frag_pos);

		// Diffuse
		float diff = max(dot(normal, light_dir), 0.0);
		vec3  diffuse = lights[i].color * diff * albedo;

		// Specular
		vec3  view_dir = normalize(viewPos - frag_pos);
		vec3  reflect_dir = reflect(-light_dir, normal);
		float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32);
		vec3  specular = lights[i].color * spec * specular_strength;

		// Apply shadow to diffuse and specular, but not ambient
		result += (diffuse + specular) * lights[i].intensity * shadow * attenuation;
	}

	return result;
}

/**
 * Apply lighting without shadows.
 * Use this for shaders that don't need shadows (sky, trails, etc.)
 */
vec3 apply_lighting_no_shadows(vec3 frag_pos, vec3 normal, vec3 albedo, float specular_strength) {
	vec3 result = ambient_light * albedo;

	for (int i = 0; i < num_lights; ++i) {
		vec3  light_dir;
		float attenuation = 1.0;

		if (lights[i].type == LIGHT_TYPE_POINT) { // Point light
			light_dir = normalize(lights[i].position - frag_pos);
			float distance = length(lights[i].position - frag_pos);
			attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * (distance * distance));
		} else if (lights[i].type == LIGHT_TYPE_DIRECTIONAL) { // Directional light
			light_dir = normalize(-lights[i].direction);
		} else if (lights[i].type == LIGHT_TYPE_SPOT) { // Spot light
			light_dir = normalize(lights[i].position - frag_pos);
			float distance = length(lights[i].position - frag_pos);
			attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * (distance * distance));
			float theta = dot(light_dir, normalize(-lights[i].direction));
			float epsilon = lights[i].inner_cutoff - lights[i].outer_cutoff;
			float intensity = clamp((theta - lights[i].outer_cutoff) / epsilon, 0.0, 1.0);
			attenuation *= intensity;
		}

		// Diffuse
		float diff = max(dot(normal, light_dir), 0.0);
		vec3  diffuse = lights[i].color * diff * albedo;

		// Specular
		vec3  view_dir = normalize(viewPos - frag_pos);
		vec3  reflect_dir = reflect(-light_dir, normal);
		float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32);
		vec3  specular = lights[i].color * spec * specular_strength;

		result += (diffuse + specular) * lights[i].intensity * attenuation;
	}

	return result;
}

#endif // HELPERS_LIGHTING_GLSL
