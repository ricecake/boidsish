#ifndef HELPERS_LIGHTING_SIMPLE_GLSL
#define HELPERS_LIGHTING_SIMPLE_GLSL

// Simple lighting without shadow support
// Use this for shaders that don't need shadows (sky, trails, etc.)

struct Light {
	vec3  position;
	float intensity;
	vec3  color;
	float padding;
};

const int   MAX_LIGHTS = 10;
const float PI = 3.14159265359;

layout(std140) uniform Lighting {
	Light lights[MAX_LIGHTS];
	int   num_lights;
	vec3  viewPos;
	float time;
};

// ============================================================================
// PBR Helper Functions for Trails
// ============================================================================

// Normal Distribution Function (GGX/Trowbridge-Reitz)
float DistributionGGX_Simple(vec3 N, vec3 H, float roughness) {
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
float GeometrySchlickGGX_Simple(float NdotV, float roughness) {
	float r = (roughness + 1.0);
	float k = (r * r) / 8.0;

	float nom = NdotV;
	float denom = NdotV * (1.0 - k) + k;

	return nom / max(denom, 0.0001);
}

// Smith's method
float GeometrySmith_Simple(vec3 N, vec3 V, vec3 L, float roughness) {
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx2 = GeometrySchlickGGX_Simple(NdotV, roughness);
	float ggx1 = GeometrySchlickGGX_Simple(NdotL, roughness);

	return ggx1 * ggx2;
}

// Fresnel-Schlick approximation
vec3 fresnelSchlick_Simple(float cosTheta, vec3 F0) {
	return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Fresnel-Schlick with roughness - for environment reflections
vec3 fresnelSchlickRoughness_Simple(float cosTheta, vec3 F0, float roughness) {
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

/**
 * Apply lighting without shadows.
 */
vec3 apply_lighting(vec3 frag_pos, vec3 normal, vec3 albedo, float ambient_strength, float specular_strength) {
	vec3 ambient = ambient_strength * albedo;
	vec3 result = ambient;

	for (int i = 0; i < num_lights; ++i) {
		// Distance attenuation
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

/**
 * PBR lighting for trails - simplified version without shadows.
 * Includes intensity boost to match main PBR lighting.
 *
 * @param frag_pos Fragment world position
 * @param normal Surface normal (must be normalized)
 * @param albedo Base color of the material
 * @param roughness Surface roughness [0=smooth/mirror, 1=rough/matte]
 * @param metallic Metallic property [0=dielectric, 1=metal]
 */
vec3 apply_lighting_pbr_simple(vec3 frag_pos, vec3 normal, vec3 albedo, float roughness, float metallic) {
	const float PBR_INTENSITY_BOOST = 4.0;

	vec3 N = normalize(normal);
	vec3 V = normalize(viewPos - frag_pos);

	// Calculate reflectance at normal incidence
	vec3 F0 = vec3(0.04);
	F0 = mix(F0, albedo, metallic);

	vec3 Lo = vec3(0.0);

	for (int i = 0; i < num_lights; ++i) {
		vec3  L = normalize(lights[i].position - frag_pos);
		vec3  H = normalize(V + L);
		float distance = length(lights[i].position - frag_pos);

		float attenuation = (lights[i].intensity * PBR_INTENSITY_BOOST) /
			(1.0 + 0.09 * distance + 0.032 * distance * distance);
		vec3 radiance = lights[i].color * attenuation;

		// Cook-Torrance BRDF
		float NDF = DistributionGGX_Simple(N, H, roughness);
		float G = GeometrySmith_Simple(N, V, L, roughness);
		vec3  F = fresnelSchlick_Simple(max(dot(H, V), 0.0), F0);

		vec3  numerator = NDF * G * F;
		float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
		vec3  specular = numerator / denominator;

		vec3 kS = F;
		vec3 kD = vec3(1.0) - kS;
		kD *= 1.0 - metallic;

		float NdotL = max(dot(N, L), 0.0);
		Lo += (kD * albedo / PI + specular) * radiance * NdotL;
	}

	// Ambient lighting with environment reflection approximation
	vec3 ambientDiffuse = vec3(0.08) * albedo;

	// Fake environment reflection for glossy surfaces
	vec3  R = reflect(-V, N);
	vec3  F0_env = mix(vec3(0.04), albedo, metallic);
	float NdotV = max(dot(N, V), 0.0);
	vec3  F_env = fresnelSchlickRoughness_Simple(NdotV, F0_env, roughness);

	// Gradient environment color
	float upAmount = R.y * 0.5 + 0.5;
	vec3  envColor = mix(vec3(0.3, 0.35, 0.4), vec3(0.6, 0.7, 0.9), smoothstep(0.0, 0.7, upAmount));

	float smoothness = 1.0 - roughness;
	float envStrength = smoothness * smoothness * 0.8;
	vec3  ambientSpecular = F_env * envColor * envStrength;

	vec3 ambient = ambientDiffuse * (1.0 - metallic * 0.9) + ambientSpecular;
	return ambient + Lo;
}

/**
 * Calculate iridescent color based on view angle and surface normal.
 * Returns a rainbow-shifted color that changes with viewing angle.
 *
 * @param view_dir Normalized view direction
 * @param normal Normalized surface normal
 * @param base_color Optional base color to blend with
 * @param time_offset Animation time for swirling effect
 */
vec3 calculate_iridescence(vec3 view_dir, vec3 normal, vec3 base_color, float time_offset) {
	// Fresnel-based angle factor
	float NdotV = abs(dot(view_dir, normal));
	float angle_factor = 1.0 - NdotV;
	angle_factor = pow(angle_factor, 2.0);

	// Thin-film interference simulation
	float thin_film = angle_factor * 6.28318; // 2*PI for full color cycle

	// Create rainbow palette with smooth transitions
	vec3 iridescent = vec3(
		sin(thin_film + 0.0) * 0.5 + 0.5,
		sin(thin_film + 2.094) * 0.5 + 0.5, // 2*PI/3
		sin(thin_film + 4.189) * 0.5 + 0.5  // 4*PI/3
	);

	// Add time-based swirl for animation
	float swirl = sin(time_offset * 0.5) * 0.5 + 0.5;
	iridescent = vec3(
		sin(angle_factor * 10.0 + swirl * 5.0) * 0.5 + 0.5,
		sin(angle_factor * 10.0 + swirl * 5.0 + 2.0) * 0.5 + 0.5,
		sin(angle_factor * 10.0 + swirl * 5.0 + 4.0) * 0.5 + 0.5
	);

	// Blend with base color based on angle
	return mix(base_color, iridescent, angle_factor * 0.8 + 0.2);
}

/**
 * PBR iridescent material - combines PBR lighting with thin-film interference.
 * Creates a metallic, color-shifting surface like oil on water or beetle shells.
 *
 * @param frag_pos Fragment world position
 * @param normal Surface normal
 * @param base_color Base/underlying color
 * @param roughness Surface roughness [0=mirror, 1=matte]
 * @param iridescence_strength How much iridescence to apply [0-1]
 */
vec3 apply_lighting_pbr_iridescent(
	vec3  frag_pos,
	vec3  normal,
	vec3  base_color,
	float roughness,
	float iridescence_strength
) {
	const float PBR_INTENSITY_BOOST = 4.0;

	vec3  N = normalize(normal);
	vec3  V = normalize(viewPos - frag_pos);
	float NdotV = max(dot(N, V), 0.0);

	// Calculate iridescent color based on view angle
	vec3 iridescent_color = calculate_iridescence(V, N, base_color, time + frag_pos.y * 2.0);

	// --- Key fix: Make iridescent color visible without requiring perfect lighting ---
	// The iridescent color should be the BASE appearance, with lighting adding highlights

	// Base iridescent appearance (always visible, angle-dependent)
	float angle_factor = 1.0 - NdotV;
	vec3  base_iridescent = iridescent_color * (0.4 + angle_factor * 0.6); // Brighter at edges

	// Add specular highlights from lights
	vec3 specular_total = vec3(0.0);

	for (int i = 0; i < num_lights; ++i) {
		vec3  L = normalize(lights[i].position - frag_pos);
		vec3  H = normalize(V + L);
		float distance = length(lights[i].position - frag_pos);
		float NdotL = max(dot(N, L), 0.0);
		float HdotV = max(dot(H, V), 0.0);

		float attenuation = (lights[i].intensity * PBR_INTENSITY_BOOST) /
			(1.0 + 0.09 * distance + 0.032 * distance * distance);
		vec3 radiance = lights[i].color * attenuation;

		// GGX specular for sharp highlights
		float NDF = DistributionGGX_Simple(N, H, roughness);
		float G = GeometrySmith_Simple(N, V, L, roughness);

		// Fresnel with iridescent F0
		vec3 F0 = iridescent_color * 0.8 + vec3(0.2); // High reflectance
		vec3 F = fresnelSchlick_Simple(HdotV, F0);

		vec3  numerator = NDF * G * F;
		float denominator = 4.0 * NdotV * NdotL + 0.0001;
		vec3  specular = numerator / denominator;

		specular_total += specular * radiance * NdotL;
	}

	// Fresnel rim - strong white/iridescent edge glow
	float fresnel_rim = pow(1.0 - NdotV, 4.0);
	vec3  rim_color = mix(vec3(1.0), iridescent_color, 0.5) * fresnel_rim * 0.8;

	// Combine: base iridescent appearance + specular highlights + rim
	vec3 final_color = base_iridescent + specular_total + rim_color;

	return final_color;
}

/**
 * Emissive glow calculation for rocket/flame trails.
 * Creates a hot, glowing effect that emits light.
 *
 * @param base_emission Base emissive color (e.g., orange for flames)
 * @param intensity Glow intensity multiplier
 * @param falloff How quickly the glow fades [0=sharp, 1=soft]
 */
vec3 calculate_emission(vec3 base_emission, float intensity, float falloff) {
	// HDR emission - can exceed 1.0 for bloom effects
	return base_emission * intensity * (1.0 + falloff);
}

#endif // HELPERS_LIGHTING_SIMPLE_GLSL
