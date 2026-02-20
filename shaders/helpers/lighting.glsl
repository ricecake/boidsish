#ifndef HELPERS_LIGHTING_GLSL
#define HELPERS_LIGHTING_GLSL

#include "../lighting.glsl"

// SDF Shadow uniforms
uniform sampler3D u_sdfTexture;
uniform vec3      u_sdfExtent;
uniform vec3      u_sdfMin;
uniform bool      u_useSdfShadow = false;

// Hi-Z depth texture for optimized screen-space shadows
uniform sampler2D u_hizTexture;

// The shader must provide these if HAS_LOCAL_POS is defined
#ifdef HAS_LOCAL_POS
// These are provided by the including shader (e.g. vis.frag)
// We declare them as uniforms or variables that will be defined before inclusion
vec3 localPos;
mat4 invModelMatrix;
#endif

const int LIGHT_TYPE_POINT = 0;
const int LIGHT_TYPE_DIRECTIONAL = 1;
const int LIGHT_TYPE_SPOT = 2;
const int LIGHT_TYPE_EMISSIVE = 3; // Glowing object light (can cast shadows)
const int LIGHT_TYPE_FLASH = 4;    // Explosion/flash light (rapid falloff)

/**
 * Calculate shadow factor for a fragment position using a specific shadow map.
 * Returns 0.0 if fully in shadow, 1.0 if fully lit.
 * Uses PCF (Percentage Closer Filtering) for soft shadow edges.
 */
float calculateShadow(int light_index, vec3 frag_pos, vec3 normal, vec3 light_dir) {
	int shadow_index = lightShadowIndices[light_index];

	// Early out for invalid indices or when no shadow lights are active
	// This MUST return before any texture operations to avoid driver issues
	if (shadow_index < 0) {
		return 1.0; // No shadow for this light
	}
	if (numShadowLights <= 0) {
		return 1.0; // No shadow maps active at all
	}

	// Handle Cascaded Shadow Maps for directional lights
	int   cascade = 0;
	float cascade_blend = 0.0; // Blend factor for smooth cascade transitions
	int   next_cascade = -1;

	if (lights[light_index].type == LIGHT_TYPE_DIRECTIONAL) {
		// Use linear depth along camera forward for more consistent splits
		float depth = dot(frag_pos - viewPos, viewDir);
		cascade = -1;
		for (int i = 0; i < MAX_CASCADES; ++i) {
			if (depth < cascadeSplits[i]) {
				cascade = i;
				break;
			}
		}

		if (cascade == -1) {
			// Beyond all cascade splits - use the last cascade as catchall
			// The far cascade is configured with extended range specifically for this
			cascade = MAX_CASCADES - 1;
		}

		// Calculate blend zone for smooth cascade transitions
		// This eliminates the harsh "perspective shift" at cascade boundaries
		// Note: Don't blend for the last cascade since it's the catchall
		if (cascade < MAX_CASCADES - 1) {
			float cascade_start = (cascade == 0) ? 0.0 : cascadeSplits[cascade - 1];
			float cascade_end = cascadeSplits[cascade];
			float cascade_range = cascade_end - cascade_start;

			// Blend zone is the last 15% of each cascade
			float blend_zone_start = cascade_end - cascade_range * 0.15;
			if (depth > blend_zone_start) {
				cascade_blend = (depth - blend_zone_start) / (cascade_end - blend_zone_start);
				cascade_blend = smoothstep(0.0, 1.0, cascade_blend); // Smooth the transition
				next_cascade = cascade + 1;
			}
		}

		shadow_index += cascade;
	}

	if (shadow_index >= MAX_SHADOW_MAPS) {
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

	// Improved bias calculation to prevent shadow acne while keeping shadows connected to geometry
	// The key insight: larger cascades have lower resolution (larger texels), so need larger bias
	// But the bias should NOT be so large that shadows appear "floating" above terrain
	float slope_factor = max(1.0 - dot(normal, light_dir), 0.0); // 0 when facing light, 1 when perpendicular

	// Base bias: very small for direct facing surfaces
	float base_bias = 0.0002;

	// Slope bias: increases for steep angles relative to light
	float slope_bias = 0.002 * slope_factor;

	// Cascade-specific bias: accounts for texel size differences
	// CRITICAL: This should be modest - previous 5x multiplier was too aggressive
	// Near cascade (0): finest resolution, minimal extra bias needed
	// Far cascade (3): coarsest resolution, but still shouldn't be huge
	float cascade_bias_scale = 1.0 + float(cascade) * 0.8; // Was 5.0, now 0.8

	// Calculate texel size in world units for this cascade (approximate)
	vec2 texel_size = 1.0 / vec2(textureSize(shadowMaps, 0).xy);

	// Final bias combines all factors
	float bias = (base_bias + slope_bias) * cascade_bias_scale;

	// Clamp to prevent over-biasing that causes disconnected shadows
	bias = clamp(bias, 0.0001, 0.01);

	// PCF - sample multiple texels for soft shadows
	// Use larger kernel for distant cascades to match their lower resolution
	float shadow = 0.0;
	int   kernel_size = (cascade < 2) ? 1 : 2; // 3x3 for near, 5x5 for far
	float sample_count = 0.0;

	for (int x = -kernel_size; x <= kernel_size; ++x) {
		for (int y = -kernel_size; y <= kernel_size; ++y) {
			vec2 offset = vec2(x, y) * texel_size;
			// sampler2DArrayShadow expects (u, v, layer, compare_value)
			vec4 shadow_coord = vec4(proj_coords.xy + offset, float(shadow_index), current_depth - bias);
			shadow += texture(shadowMaps, shadow_coord);
			sample_count += 1.0;
		}
	}
	shadow /= sample_count;

	// Blend with next cascade if in transition zone
	// This smooths the visual transition and eliminates the "perspective shift" artifact
	if (cascade_blend > 0.0 && next_cascade >= 0 && next_cascade < MAX_CASCADES) {
		int next_shadow_index = shadow_index - cascade + next_cascade;
		if (next_shadow_index < MAX_SHADOW_MAPS) {
			// Sample from next cascade with its own bias
			float next_cascade_bias_scale = 1.0 + float(next_cascade) * 0.8;
			float next_bias = (base_bias + slope_bias) * next_cascade_bias_scale;
			next_bias = clamp(next_bias, 0.0001, 0.01);

			// Transform to next cascade's light space
			vec4 next_frag_pos_light_space = lightSpaceMatrices[next_shadow_index] * vec4(frag_pos, 1.0);
			if (abs(next_frag_pos_light_space.w) > 0.0001) {
				vec3 next_proj_coords = next_frag_pos_light_space.xyz / next_frag_pos_light_space.w;
				next_proj_coords = next_proj_coords * 0.5 + 0.5;

				// Only blend if also within next cascade's valid range
				if (next_proj_coords.x >= 0.0 && next_proj_coords.x <= 1.0 && next_proj_coords.y >= 0.0 &&
				    next_proj_coords.y <= 1.0 && next_proj_coords.z >= 0.0 && next_proj_coords.z <= 1.0) {
					float next_shadow = 0.0;
					int   next_kernel_size = (next_cascade < 2) ? 1 : 2;
					float next_sample_count = 0.0;

					for (int x = -next_kernel_size; x <= next_kernel_size; ++x) {
						for (int y = -next_kernel_size; y <= next_kernel_size; ++y) {
							vec2 offset = vec2(x, y) * texel_size;
							vec4 shadow_coord = vec4(
								next_proj_coords.xy + offset,
								float(next_shadow_index),
								next_proj_coords.z - next_bias
							);
							next_shadow += texture(shadowMaps, shadow_coord);
							next_sample_count += 1.0;
						}
					}
					next_shadow /= next_sample_count;

					// Blend between cascades
					shadow = mix(shadow, next_shadow, cascade_blend);
				}
			}
		}
	}

	return shadow;
}

/**
 * Calculate SDF shadow factor for a fragment using the per-instance SDF.
 * This provides self-shadowing and contact shadows for decor.
 */
float calculateSdfShadow(vec3 frag_pos, vec3 light_dir) {
	if (!u_useSdfShadow)
		return 1.0;

	// Transform fragment position to local SDF space [0, 1]
	// Note: We assume frag_pos is already in local space if called from decor shader,
	// but the decor shader currently uses world space for lighting.
	// We need to pass the model matrix inverse or transform it in the shader.
	// For now, let's assume frag_pos passed here is world space and we have local_pos.

	// Raymarch towards the light
	float shadow = 1.0;
	float t = 0.02; // Start slightly away from surface
	for (int i = 0; i < 32; ++i) {
		vec3 p = frag_pos + light_dir * t;

		// Transform p to local model space
		// (This requires the inverse model matrix which we don't have easily in this header)
		// Wait, if we are in the decor shader, we can pass the local position.

		// For now, a simplified version that assumes we have local coordinates.
		// I will update the caller to pass local coordinates.

		vec3 local_p = (p - u_sdfMin) / u_sdfExtent;
		if (any(lessThan(local_p, vec3(0.0))) || any(greaterThan(local_p, vec3(1.0)))) {
			break; // Left the SDF volume
		}

		float dist = texture(u_sdfTexture, local_p).r * length(u_sdfExtent);
		if (dist < 0.01) {
			return 0.0; // Occluded
		}

		// Soft shadows:
		shadow = min(shadow, 10.0 * dist / t);

		t += max(0.01, dist);
		if (t > 2.0) break; // Limit ray length
	}

	return clamp(shadow, 0.0, 1.0);
}

/**
 * Calculate the relative luminance of a color.
 * Used for determining how much a specular highlight should contribute to fragment opacity.
 */
float get_luminance(vec3 color) {
	return dot(color, vec3(0.2126, 0.7152, 0.0722));
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

// ============================================================================
// Attenuation Helpers for Light Types
// ============================================================================

/**
 * Calculate light direction and attenuation for any light type.
 * @param light_index Index into the lights array
 * @param frag_pos Fragment world position
 * @param light_dir Output: normalized direction from fragment to light
 * @param attenuation Output: combined distance and angular attenuation
 */
void calculateLightContribution(int light_index, vec3 frag_pos, out vec3 light_dir, out float attenuation) {
	attenuation = 1.0;

	if (lights[light_index].type == LIGHT_TYPE_POINT) {
		// Point light: attenuates with distance
		light_dir = normalize(lights[light_index].position - frag_pos);
		float distance = length(lights[light_index].position - frag_pos);
		// Practical attenuation curve (inverse square falloff with linear term)
		attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);

	} else if (lights[light_index].type == LIGHT_TYPE_DIRECTIONAL) {
		// Directional light: no attenuation, parallel rays
		light_dir = normalize(-lights[light_index].direction);
		attenuation = 1.0;

	} else if (lights[light_index].type == LIGHT_TYPE_SPOT) {
		// Spot light: distance attenuation + angular falloff
		light_dir = normalize(lights[light_index].position - frag_pos);
		float distance = length(lights[light_index].position - frag_pos);
		attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);

		// Angular falloff using inner/outer cutoff angles
		float theta = dot(light_dir, normalize(-lights[light_index].direction));
		float epsilon = lights[light_index].inner_cutoff - lights[light_index].outer_cutoff;
		float angular_intensity = clamp((theta - lights[light_index].outer_cutoff) / epsilon, 0.0, 1.0);
		attenuation *= angular_intensity;

	} else if (lights[light_index].type == LIGHT_TYPE_EMISSIVE) {
		// Emissive/glowing object light: similar to point light but with soft near-field
		// inner_cutoff stores the emissive object radius for soft falloff
		light_dir = normalize(lights[light_index].position - frag_pos);
		float distance = length(lights[light_index].position - frag_pos);
		float emissive_radius = lights[light_index].inner_cutoff;

		// Soft falloff that accounts for the size of the glowing object
		// Avoids harsh falloff when very close to the light source
		float effective_dist = max(distance - emissive_radius * 0.5, 0.0);
		attenuation = 1.0 / (1.0 + 0.09 * effective_dist + 0.032 * effective_dist * effective_dist);

		// Boost intensity when inside or near the emissive radius
		float proximity_boost = smoothstep(emissive_radius * 2.0, 0.0, distance);
		attenuation = mix(attenuation, 1.0, proximity_boost * 0.5);

	} else if (lights[light_index].type == LIGHT_TYPE_FLASH) {
		// Flash/explosion light: very bright with rapid falloff
		// inner_cutoff = flash radius, outer_cutoff = falloff exponent
		light_dir = normalize(lights[light_index].position - frag_pos);
		float distance = length(lights[light_index].position - frag_pos);
		float flash_radius = lights[light_index].inner_cutoff;
		float falloff_exp = lights[light_index].outer_cutoff;

		// Normalized distance (0 at center, 1 at radius edge)
		float norm_dist = distance / max(flash_radius, 0.001);

		// Sharp inverse-power falloff for explosive effect
		// Falls off rapidly but smoothly
		attenuation = 1.0 / pow(1.0 + norm_dist, falloff_exp);

		// Hard cutoff at 2x radius to prevent distant influence
		attenuation *= smoothstep(2.0, 1.5, norm_dist);
	}
}

// ============================================================================
// PBR Lighting Functions
// ============================================================================

// PBR intensity multiplier to compensate for energy conservation
// PBR is inherently darker than legacy Phong, so we boost it
const float PBR_INTENSITY_BOOST = 4.0;

/**
 * PBR lighting with Cook-Torrance BRDF - supports all light types.
 * Returns vec4(color.rgb, specular_luminance).
 *
 * @param frag_pos Fragment world position
 * @param normal Surface normal (must be normalized)
 * @param albedo Base color of the material
 * @param roughness Surface roughness [0=smooth, 1=rough]
 * @param metallic Metallic property [0=dielectric, 1=metal]
 * @param ao Ambient occlusion [0=fully occluded, 1=no occlusion]
 */
vec4 apply_lighting_pbr(vec3 frag_pos, vec3 normal, vec3 albedo, float roughness, float metallic, float ao) {
	vec3 N = normalize(normal);
	vec3 V = normalize(viewPos - frag_pos);

	// Calculate reflectance at normal incidence
	// For dielectrics use 0.04, for metals use albedo color
	vec3 F0 = vec3(0.04);
	F0 = mix(F0, albedo, metallic);

	vec3  Lo = vec3(0.0);
	float spec_lum = 0.0;

	for (int i = 0; i < num_lights; ++i) {
		// Get light direction and attenuation based on light type
		vec3  L;
		float base_attenuation;
		calculateLightContribution(i, frag_pos, L, base_attenuation);

		vec3  H = normalize(V + L);
		float distance = length(lights[i].position - frag_pos);

		// For PBR, we apply intensity boost to compensate for energy conservation
		// Note: directional lights don't use distance attenuation
		float attenuation;
		if (lights[i].type == LIGHT_TYPE_DIRECTIONAL) {
			attenuation = lights[i].intensity * PBR_INTENSITY_BOOST;
		} else {
			attenuation = (lights[i].intensity * PBR_INTENSITY_BOOST) * base_attenuation;
		}
		vec3 radiance = lights[i].color * attenuation;

		// Cook-Torrance BRDF
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

		// Calculate shadow with slope-scaled bias
		float shadow = calculateShadow(i, frag_pos, N, L);

		// Apply SDF shadow if enabled
		if (u_useSdfShadow) {
			// For SDF shadow, we use the local position
			// This requires vis.frag to set a global variable or pass it
			// We'll use a hack: if LocalPos is available, we use it.
			// But lighting.glsl is a header.
			// Let's assume there's a global variable 'localPos' defined by the caller.
#ifdef HAS_LOCAL_POS
			shadow *= calculateSdfShadow(localPos, normalize(mat3(invModelMatrix) * L));
#endif
		}

		// Add to outgoing radiance Lo
		vec3 specular_radiance = specular * radiance * NdotL * shadow;
		Lo += (kD * albedo / PI) * radiance * NdotL * shadow + specular_radiance;
		spec_lum += get_luminance(specular_radiance);
	}

	// Ambient lighting for PBR (uses ambient_light uniform from main branch)
	vec3 ambientDiffuse = ambient_light * albedo * ao;

	// Environment reflection approximation for glossy surfaces
	vec3 R = reflect(-V, N);

	// Fresnel at grazing angles - smooth surfaces reflect more at edges
	vec3  F0_env = mix(vec3(0.04), albedo, metallic);
	float NdotV = max(dot(N, V), 0.0);
	vec3  F_env = fresnelSchlickRoughness(NdotV, F0_env, roughness);

	// Fake environment color - gradient from horizon to sky
	float upAmount = R.y * 0.5 + 0.5;
	vec3  envColor = mix(
        vec3(0.3, 0.35, 0.4), // Horizon/ground color
        vec3(0.6, 0.7, 0.9),  // Sky color
        smoothstep(0.0, 0.7, upAmount)
    );

	// Environment reflection strength based on smoothness
	float smoothness = 1.0 - roughness;
	float envStrength = smoothness * smoothness * 0.8;

	// Metallic surfaces should reflect the environment color tinted by albedo
	// Non-metallic surfaces reflect environment but less strongly
	vec3 ambientSpecular = F_env * envColor * envStrength * ao;

	// Combine diffuse and specular ambient
	vec3 ambient = ambientDiffuse * (1.0 - metallic * 0.9) + ambientSpecular;
	vec3 color = ambient + Lo;

	return vec4(color, spec_lum + get_luminance(ambientSpecular));
}

/**
 * PBR lighting without shadows - for shaders that don't need shadow calculations.
 * Supports all light types (point, directional, spot).
 * Returns vec4(color.rgb, specular_luminance).
 */
vec4 apply_lighting_pbr_no_shadows(vec3 frag_pos, vec3 normal, vec3 albedo, float roughness, float metallic, float ao) {
	vec3 N = normalize(normal);
	vec3 V = normalize(viewPos - frag_pos);

	vec3 F0 = vec3(0.04);
	F0 = mix(F0, albedo, metallic);

	vec3  Lo = vec3(0.0);
	float spec_lum = 0.0;

	for (int i = 0; i < num_lights; ++i) {
		vec3  L;
		float base_attenuation;
		calculateLightContribution(i, frag_pos, L, base_attenuation);

		vec3 H = normalize(V + L);

		float attenuation;
		if (lights[i].type == LIGHT_TYPE_DIRECTIONAL) {
			attenuation = lights[i].intensity * PBR_INTENSITY_BOOST;
		} else {
			attenuation = (lights[i].intensity * PBR_INTENSITY_BOOST) * base_attenuation;
		}
		vec3 radiance = lights[i].color * attenuation;

		float NDF = DistributionGGX(N, H, roughness);
		float G = GeometrySmith(N, V, L, roughness);
		vec3  F = fresnelSchlick(max(dot(H, V), 0.0), F0);

		vec3  numerator = NDF * G * F;
		float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
		vec3  specular = numerator / denominator;

		vec3 kS = F;
		vec3 kD = vec3(1.0) - kS;
		kD *= 1.0 - metallic;

		float NdotL = max(dot(N, L), 0.0);

		vec3 specular_radiance = specular * radiance * NdotL;
		Lo += (kD * albedo / PI) * radiance * NdotL + specular_radiance;
		spec_lum += get_luminance(specular_radiance);
	}

	// Ambient (same as shadowed version)
	vec3  ambientDiffuse = ambient_light * albedo * ao;
	vec3  R = reflect(-V, N);
	vec3  F0_env = mix(vec3(0.04), albedo, metallic);
	float NdotV = max(dot(N, V), 0.0);
	vec3  F_env = fresnelSchlickRoughness(NdotV, F0_env, roughness);
	float upAmount = R.y * 0.5 + 0.5;
	vec3  envColor = mix(vec3(0.3, 0.35, 0.4), vec3(0.6, 0.7, 0.9), smoothstep(0.0, 0.7, upAmount));
	float smoothness = 1.0 - roughness;
	float envStrength = smoothness * smoothness * 0.8;
	vec3  ambientSpecular = F_env * envColor * envStrength * ao;
	vec3  ambient = ambientDiffuse * (1.0 - metallic * 0.9) + ambientSpecular;

	return vec4(ambient + Lo, spec_lum + get_luminance(ambientSpecular));
}

// ============================================================================
// Legacy/Phong Lighting Functions
// ============================================================================

/**
 * Apply lighting with shadow support - supports all light types.
 * Returns vec4(color.rgb, specular_luminance).
 */
vec4 apply_lighting(vec3 frag_pos, vec3 normal, vec3 albedo, float specular_strength) {
	vec3  result = ambient_light * albedo;
	float spec_lum = 0.0;

	for (int i = 0; i < num_lights; ++i) {
		vec3  light_dir;
		float attenuation;
		calculateLightContribution(i, frag_pos, light_dir, attenuation);

		// Calculate shadow factor for this light with slope-scaled bias
		float shadow = calculateShadow(i, frag_pos, normal, light_dir);

		// Apply SDF shadow if enabled
		if (u_useSdfShadow) {
#ifdef HAS_LOCAL_POS
			shadow *= calculateSdfShadow(localPos, normalize(mat3(invModelMatrix) * light_dir));
#endif
		}

		// Diffuse
		float diff = max(dot(normal, light_dir), 0.0);
		vec3  diffuse = lights[i].color * diff * albedo;

		// Specular (Blinn-Phong)
		vec3  view_dir = normalize(viewPos - frag_pos);
		vec3  reflect_dir = reflect(-light_dir, normal);
		float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32);
		vec3  specular_contribution = lights[i].color * spec * specular_strength * lights[i].intensity * shadow *
			attenuation;

		// Apply shadow and attenuation to diffuse and specular, but not ambient
		result += (diffuse * lights[i].intensity * shadow * attenuation) + specular_contribution;
		spec_lum += get_luminance(specular_contribution);
	}

	return vec4(result, spec_lum);
}

/**
 * Apply lighting without shadows - supports all light types.
 * Returns vec4(color.rgb, specular_luminance).
 */
vec4 apply_lighting_no_shadows(vec3 frag_pos, vec3 normal, vec3 albedo, float specular_strength) {
	vec3  result = ambient_light * albedo;
	float spec_lum = 0.0;

	for (int i = 0; i < num_lights; ++i) {
		vec3  light_dir;
		float attenuation;
		calculateLightContribution(i, frag_pos, light_dir, attenuation);

		// Diffuse
		float diff = max(dot(normal, light_dir), 0.0);
		vec3  diffuse = lights[i].color * diff * albedo;

		// Specular
		vec3  view_dir = normalize(viewPos - frag_pos);
		vec3  reflect_dir = reflect(-light_dir, normal);
		float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32);
		vec3  specular_contribution = lights[i].color * spec * specular_strength * lights[i].intensity * attenuation;

		result += (diffuse * lights[i].intensity * attenuation) + specular_contribution;
		spec_lum += get_luminance(specular_contribution);
	}

	return vec4(result, spec_lum);
}

// ============================================================================
// Iridescent/Special Effect Lighting
// ============================================================================

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

	// Add time-based swirl for animation
	float swirl = sin(time_offset * 0.5) * 0.5 + 0.5;
	vec3  iridescent = vec3(
        sin(angle_factor * 10.0 + swirl * 5.0) * 0.5 + 0.5,
        sin(angle_factor * 10.0 + swirl * 5.0 + 2.0) * 0.5 + 0.5,
        sin(angle_factor * 10.0 + swirl * 5.0 + 4.0) * 0.5 + 0.5
    );

	// Blend with base color based on angle
	return mix(base_color, iridescent, angle_factor * 0.8 + 0.2);
}

/**
 * PBR iridescent material - combines PBR lighting with thin-film interference.
 * Returns vec4(color.rgb, specular_luminance).
 *
 * @param frag_pos Fragment world position
 * @param normal Surface normal
 * @param base_color Base/underlying color
 * @param roughness Surface roughness [0=mirror, 1=matte]
 * @param iridescence_strength How much iridescence to apply [0-1]
 */
vec4 apply_lighting_pbr_iridescent_no_shadows(
	vec3  frag_pos,
	vec3  normal,
	vec3  base_color,
	float roughness,
	float iridescence_strength
) {
	vec3  N = normalize(normal);
	vec3  V = normalize(viewPos - frag_pos);
	float NdotV = max(dot(N, V), 0.0);

	// Calculate iridescent color based on view angle
	vec3 iridescent_color = calculate_iridescence(V, N, base_color, time + frag_pos.y * 2.0);

	// Base iridescent appearance (always visible, angle-dependent)
	float angle_factor = 1.0 - NdotV;
	vec3  base_iridescent = iridescent_color * (0.4 + angle_factor * 0.6);

	// Add specular highlights from lights
	vec3  specular_total = vec3(0.0);
	float spec_lum = 0.0;

	for (int i = 0; i < num_lights; ++i) {
		vec3  L;
		float base_attenuation;
		calculateLightContribution(i, frag_pos, L, base_attenuation);

		vec3  H = normalize(V + L);
		float NdotL = max(dot(N, L), 0.0);
		float HdotV = max(dot(H, V), 0.0);

		float attenuation;
		if (lights[i].type == LIGHT_TYPE_DIRECTIONAL) {
			attenuation = lights[i].intensity * PBR_INTENSITY_BOOST;
		} else {
			attenuation = (lights[i].intensity * PBR_INTENSITY_BOOST) * base_attenuation;
		}
		vec3 radiance = lights[i].color * attenuation;

		// GGX specular for sharp highlights
		float NDF = DistributionGGX(N, H, roughness);
		float G = GeometrySmith(N, V, L, roughness);

		// Fresnel with iridescent F0
		vec3 F0 = iridescent_color * 0.8 + vec3(0.2);
		vec3 F = fresnelSchlick(HdotV, F0);

		vec3  numerator = NDF * G * F;
		float denominator = 4.0 * NdotV * NdotL + 0.0001;
		vec3  specular = numerator / denominator;

		vec3 specular_contribution = specular * radiance * NdotL;
		specular_total += specular_contribution;
		spec_lum += get_luminance(specular_contribution);
	}

	// Fresnel rim - strong white/iridescent edge glow
	float fresnel_rim = pow(1.0 - NdotV, 4.0);
	vec3  rim_color = mix(vec3(1.0), iridescent_color, 0.5) * fresnel_rim * 0.8;

	// Combine: base iridescent appearance + specular highlights + rim
	return vec4(base_iridescent + specular_total + rim_color, spec_lum + get_luminance(rim_color));
}

// ============================================================================
// Emissive and Flash Effect Functions
// ============================================================================

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

/**
 * Render a glowing/emissive object surface.
 * Combines emissive self-illumination with optional environmental lighting.
 * Use this for objects that ARE the light source (lamps, magic orbs, etc.)
 * Returns vec4(color.rgb, specular_luminance).
 *
 * @param frag_pos Fragment world position
 * @param normal Surface normal
 * @param emissive_color The glow color of the object
 * @param emissive_intensity How bright the glow is (can exceed 1.0 for HDR/bloom)
 * @param base_albedo Optional base color for non-emissive parts
 * @param emissive_coverage How much of surface is emissive [0=none, 1=fully glowing]
 */
vec4 apply_emissive_surface(
	vec3  frag_pos,
	vec3  normal,
	vec3  emissive_color,
	float emissive_intensity,
	vec3  base_albedo,
	float emissive_coverage
) {
	vec3 N = normalize(normal);
	vec3 V = normalize(viewPos - frag_pos);

	// The emissive part - self-illuminated, not affected by external lighting
	vec3 emission = emissive_color * emissive_intensity;

	// Add subtle Fresnel glow at edges for a more volumetric feel
	float fresnel = pow(1.0 - max(dot(N, V), 0.0), 3.0);
	emission += emissive_color * fresnel * emissive_intensity * 0.5;

	// The non-emissive part gets regular lighting
	vec4 lit_surface = vec4(0.0);
	if (emissive_coverage < 1.0) {
		lit_surface = apply_lighting_no_shadows(frag_pos, normal, base_albedo, 0.5);
	}

	// Blend between emissive and lit surface
	// Emissions are considered fully opaque specular-like contributors for alpha purposes
	return mix(lit_surface, vec4(emission, get_luminance(emission)), emissive_coverage);
}

/**
 * Render a glowing object with PBR properties for non-emissive regions.
 * The emissive parts glow while other parts use PBR lighting.
 * Returns vec4(color.rgb, specular_luminance).
 *
 * @param frag_pos Fragment world position
 * @param normal Surface normal
 * @param emissive_color The glow color
 * @param emissive_intensity Glow brightness (box HDR, can exceed 1.0)
 * @param base_albedo Base color for non-emissive parts
 * @param roughness PBR roughness for non-emissive parts
 * @param metallic PBR metallic for non-emissive parts
 * @param emissive_mask Per-fragment emissive coverage [0-1]
 */
vec4 apply_emissive_surface_pbr(
	vec3  frag_pos,
	vec3  normal,
	vec3  emissive_color,
	float emissive_intensity,
	vec3  base_albedo,
	float roughness,
	float metallic,
	float emissive_mask
) {
	vec3 N = normalize(normal);
	vec3 V = normalize(viewPos - frag_pos);

	// Emissive component
	vec3  emission = emissive_color * emissive_intensity;
	float fresnel = pow(1.0 - max(dot(N, V), 0.0), 3.0);
	emission += emissive_color * fresnel * emissive_intensity * 0.3;

	// PBR lit component for non-emissive parts
	vec4 pbr_lit = apply_lighting_pbr_no_shadows(frag_pos, normal, base_albedo, roughness, metallic, 1.0);

	// Blend based on emissive mask
	return mix(pbr_lit, vec4(emission, get_luminance(emission)), emissive_mask);
}

/**
 * Calculate flash/explosion illumination contribution to a surface.
 * Call this in addition to regular lighting for surfaces hit by a flash.
 * Returns additive light contribution.
 *
 * @param frag_pos Fragment world position
 * @param normal Surface normal
 * @param flash_pos Explosion/flash center position
 * @param flash_color Flash color (typically warm white/orange)
 * @param flash_intensity Flash brightness (can be very high, e.g., 10-50)
 * @param flash_radius Effective radius of the flash
 * @param flash_time Normalized time since flash [0=peak, 1=faded]
 */
vec3 calculate_flash_contribution(
	vec3  frag_pos,
	vec3  normal,
	vec3  flash_pos,
	vec3  flash_color,
	float flash_intensity,
	float flash_radius,
	float flash_time
) {
	vec3  L = normalize(flash_pos - frag_pos);
	float distance = length(flash_pos - frag_pos);
	float NdotL = max(dot(normalize(normal), L), 0.0);

	// Distance attenuation with sharp falloff
	float norm_dist = distance / max(flash_radius, 0.001);
	float dist_atten = 1.0 / pow(1.0 + norm_dist, 2.0);
	dist_atten *= smoothstep(2.0, 1.0, norm_dist); // Hard cutoff

	// Time-based fade (flash decays rapidly)
	// Starts bright, fades quickly, with slight persistence
	float time_atten = pow(1.0 - clamp(flash_time, 0.0, 1.0), 3.0);

	// Combine for final flash contribution
	return flash_color * flash_intensity * dist_atten * time_atten * NdotL;
}

/**
 * Full flash effect - returns both the flash illumination and suggested bloom.
 * Use the bloom value to drive post-processing bloom intensity.
 *
 * @param frag_pos Fragment world position
 * @param normal Surface normal
 * @param albedo Surface base color
 * @param flash_pos Flash center
 * @param flash_color Flash color
 * @param flash_intensity Flash brightness
 * @param flash_radius Flash radius
 * @param flash_time Normalized time [0=peak, 1=faded]
 * @param out_bloom Output: suggested bloom intensity for post-processing
 */
vec3 apply_flash_lighting(
	vec3      frag_pos,
	vec3      normal,
	vec3      albedo,
	vec3      flash_pos,
	vec3      flash_color,
	float     flash_intensity,
	float     flash_radius,
	float     flash_time,
	out float out_bloom
) {
	vec3 flash = calculate_flash_contribution(
		frag_pos,
		normal,
		flash_pos,
		flash_color,
		flash_intensity,
		flash_radius,
		flash_time
	);

	// Flash contribution adds to surface color
	vec3 result = albedo * flash;

	// Calculate bloom intensity based on flash brightness at this point
	// Surfaces closer to flash should bloom more
	out_bloom = length(flash) * 0.1; // Scale for bloom post-process

	return result;
}

#endif // HELPERS_LIGHTING_GLSL
