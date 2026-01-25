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

// ============================================================================
// PBR Functions (Cook-Torrance BRDF)
// ============================================================================

const float PI = 3.14159265359;

// Normal Distribution Function (GGX/Trowbridge-Reitz)
float DistributionGGX(vec3 N, vec3 H, float roughness) {
	// Clamp roughness to avoid singularity at 0 (causes black surfaces)
	float r = max(roughness, 0.04);
	float a = r * r;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0.0);
	float NdotH2 = NdotH * NdotH;

	float nom = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;

	return nom / max(denom, 0.0001);
}

// Geometry function (Schlick-GGX)
float GeometrySchlickGGX(float NdotV, float roughness) {
	float r = (roughness + 1.0);
	float k = (r * r) / 8.0;

	float nom = NdotV;
	float denom = NdotV * (1.0 - k) + k;

	return nom / max(denom, 0.0001);
}

// Smith's method for geometry obstruction and shadowing
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx2 = GeometrySchlickGGX(NdotV, roughness);
	float ggx1 = GeometrySchlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}

// Fresnel-Schlick approximation
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
	return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Fresnel-Schlick with roughness - for environment reflections
// Rough surfaces have less pronounced Fresnel effect
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

/**
 * PBR lighting with Cook-Torrance BRDF.
 *
 * @param frag_pos Fragment world position
 * @param normal Surface normal (must be normalized)
 * @param albedo Base color of the material
 * @param roughness Surface roughness [0=smooth, 1=rough]
 * @param metallic Metallic property [0=dielectric, 1=metal]
 * @param ao Ambient occlusion [0=fully occluded, 1=no occlusion]
 */
vec3 apply_lighting_pbr(vec3 frag_pos, vec3 normal, vec3 albedo, float roughness, float metallic, float ao) {
	vec3 N = normalize(normal);
	vec3 V = normalize(viewPos - frag_pos);

	// Calculate reflectance at normal incidence
	// For dielectrics use 0.04, for metals use albedo color
	vec3 F0 = vec3(0.04);
	F0 = mix(F0, albedo, metallic);

	// Outgoing radiance
	vec3 Lo = vec3(0.0);

	for (int i = 0; i < num_lights; ++i) {
		vec3  L;
		float attenuation = 1.0;

		if (lights[i].type == LIGHT_TYPE_POINT) { // Point light
			L = normalize(lights[i].position - frag_pos);
			float distance = length(lights[i].position - frag_pos);
			attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * (distance * distance));
		} else if (lights[i].type == LIGHT_TYPE_DIRECTIONAL) { // Directional light
			L = normalize(-lights[i].direction);
			// Directional lights have no distance attenuation
		} else if (lights[i].type == LIGHT_TYPE_SPOT) { // Spot light
			L = normalize(lights[i].position - frag_pos);
			float distance = length(lights[i].position - frag_pos);
			attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * (distance * distance));
			float theta = dot(L, normalize(-lights[i].direction));
			float epsilon = lights[i].inner_cutoff - lights[i].outer_cutoff;
			float intensity_spot = clamp((theta - lights[i].outer_cutoff) / epsilon, 0.0, 1.0);
			attenuation *= intensity_spot;
		}

		// Per-light radiance (light color * intensity)
		vec3 radiance = lights[i].color * lights[i].intensity;

		// Cook-Torrance BRDF
		vec3  H = normalize(V + L);
		float NDF = DistributionGGX(N, H, roughness);
		float G = GeometrySmith(N, V, L, roughness);
		vec3  F = fresnelSchlick(max(dot(H, V), 0.0), F0);

		vec3  numerator = NDF * G * F;
		float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
		vec3  specular = numerator / denominator;

		// kS is Fresnel (specular component)
		vec3 kS = F;
		// kD is diffuse component (energy conservation: kD + kS = 1.0)
		vec3 kD = vec3(1.0) - kS;
		// Metals don't have diffuse lighting
		kD *= 1.0 - metallic;

		float NdotL = max(dot(N, L), 0.0);

		// Calculate shadow
		float shadow = calculateShadow(lightShadowIndices[i], frag_pos);

		// Add to outgoing radiance Lo
		Lo += (kD * albedo / PI + specular) * radiance * NdotL * shadow * attenuation;
	}

	// Ambient lighting using the global ambient term
	vec3 ambient = ambient_light * albedo * ao;
	vec3 color = ambient + Lo;

	return color;
}

/**
 * Apply lighting with shadow support.
 * This version checks each light for shadow casting capability.
 */
vec3 apply_lighting(vec3 frag_pos, vec3 normal, vec3 albedo, float ambient_strength, float specular_strength) {
	vec3 ambient = ambient_strength * albedo;
	vec3 result = ambient;

	for (int i = 0; i < num_lights; ++i) {
		// Calculate shadow factor for this light
		float shadow = calculateShadow(lightShadowIndices[i], frag_pos);

		// Distance attenuation (prevents oversaturation at close range)
		float distance = length(lights[i].position - frag_pos);
		float attenuation = lights[i].intensity / (1.0 + 0.045 * distance + 0.0075 * distance * distance);

		// Diffuse
		vec3  light_dir = normalize(lights[i].position - frag_pos);
		float diff = max(dot(normal, light_dir), 0.0);
		vec3  diffuse = lights[i].color * diff * albedo;

		// Specular
		vec3  view_dir = normalize(viewPos - frag_pos);
		vec3  reflect_dir = reflect(-light_dir, normal);
		float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32);
		vec3  specular = lights[i].color * spec * specular_strength;

		// Apply shadow and attenuation to diffuse and specular, but not ambient
		result += (diffuse + specular) * attenuation * shadow;
	}

	return result;
}

/**
 * Legacy apply_lighting without shadows (for shaders that don't need them).
 * Use apply_lighting_no_shadows explicitly if you want to skip shadow calculations.
 */
vec3 apply_lighting_no_shadows(
	vec3  frag_pos,
	vec3  normal,
	vec3  albedo,
	float ambient_strength,
	float specular_strength
) {
	vec3 ambient = ambient_strength * albedo;
	vec3 result = ambient;

	for (int i = 0; i < num_lights; ++i) {
		// Distance attenuation (prevents oversaturation at close range)
		float distance = length(lights[i].position - frag_pos);
		float attenuation = lights[i].intensity / (1.0 + 0.045 * distance + 0.0075 * distance * distance);

		// Diffuse
		vec3  light_dir = normalize(lights[i].position - frag_pos);
		float diff = max(dot(normal, light_dir), 0.0);
		vec3  diffuse = lights[i].color * diff * albedo;

		// Specular
		vec3  view_dir = normalize(viewPos - frag_pos);
		vec3  reflect_dir = reflect(-light_dir, normal);
		float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32);
		vec3  specular = lights[i].color * spec * specular_strength;

		result += (diffuse + specular) * attenuation;
	}

	return result;
}

#endif // HELPERS_LIGHTING_GLSL
