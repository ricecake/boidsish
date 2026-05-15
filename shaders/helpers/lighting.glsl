#ifndef HELPERS_LIGHTING_GLSL
#define HELPERS_LIGHTING_GLSL

#include "helpers/constants.glsl"
#include "helpers/microfacet_glinting.glsl"
#include "../lighting.glsl"
#include "clouds.glsl"
#include "brdf.glsl"

// Atmosphere constants for transmittance lookup
const float kEarthRadiusKM = 6360.0;

#ifndef ATMOSPHERE_HEIGHT_DEFINED
	#define ATMOSPHERE_HEIGHT_DEFINED
uniform float u_atmosphereHeight; // usually 100.0 km
#endif

#ifndef TRANSMITTANCE_LUT_DEFINED
	#define TRANSMITTANCE_LUT_DEFINED
layout(binding = [[ATMOSPHERE_TRANSMITTANCE_BINDING]]) uniform sampler2D u_transmittanceLUT;
#endif

layout(binding = [[ATMOSPHERE_CLOUD_SHADOW_BINDING]]) uniform sampler2D u_cloudShadowMap;
#ifndef TERRAIN_GRID_DEFINED
	#define TERRAIN_GRID_DEFINED
layout(binding = [[TERRAIN_CHUNK_GRID_BINDING]]) uniform isampler2D u_chunkGrid;
#endif

/**
 * Maps height and sun cosine angle to UV coordinates for the transmittance LUT.
 * Matches logic in atmosphere/common.glsl but standalone here for convenience.
 */
vec2 getTransmittanceUV(float r, float mu) {
	float x_mu = mu * 0.5 + 0.5;
	float x_r = (r - kEarthRadiusKM) / max(u_atmosphereHeight, 1.0);
	return vec2(clamp(x_mu, 0.0, 1.0), clamp(x_r, 0.0, 1.0));
}

const int LIGHT_TYPE_POINT = 0;
const int LIGHT_TYPE_DIRECTIONAL = 1;
const int LIGHT_TYPE_SPOT = 2;
const int LIGHT_TYPE_EMISSIVE = 3; // Glowing object light (can cast shadows)
const int LIGHT_TYPE_FLASH = 4;    // Explosion/flash light (rapid falloff)

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

/**
 * Calculate cloud shadow factor for a fragment position.
 * Projects the fragment position to the cloud layer along the light direction.
 */
float calculateCloudShadow(int light_index, vec3 frag_pos) {
	if (lights[light_index].type != LIGHT_TYPE_DIRECTIONAL || cloudShadowIntensity <= 0.0) {
		return 1.0;
	}

	vec3 L = normalize(-lights[light_index].direction);
	if (L.y <= 0.0)
		return 1.0;

	// Project to the middle of the cloud layer to ensure we're within the tapering range
	float shadowAltitude = cloudAltitude + cloudThickness * 0.5;
	float scaledCloudAltitude = shadowAltitude * worldScale;
	float t = (scaledCloudAltitude - frag_pos.y) / L.y;

	if (t < 0.0)
		return 1.0;

	vec3 cloudPos = frag_pos + L * t;

	// Use the precomputed 2D shadow map
	vec4 shadowUV = cloudShadowMatrix * vec4(cloudPos.xz, 0.0, 1.0);

	// Sample the shadow map
	float d = 0.0;
	if (shadowUV.x >= 0.0 && shadowUV.x <= 1.0 && shadowUV.y >= 0.0 && shadowUV.y <= 1.0) {
		d = texture(u_cloudShadowMap, shadowUV.xy).r;
	} else {
		// Fallback for points outside the shadow map: evaluate noise directly
		d = evaluateCloudShadowDensityAtWorldPos(cloudPos.xz, time);
	}

	return mix(1.0, exp(-d), cloudShadowIntensity);
}

#ifdef USE_TERRAIN_DATA
// Forward declare terrain shadow coverage from terrain_shadows.glsl
float terrainShadowCoverage(vec3 worldPos, vec3 normal, vec3 lightDir);
#else
// Fallback if terrain data is not available
float terrainShadowCoverage(vec3 worldPos, vec3 normal, vec3 lightDir) {
	return 1.0;
}
#endif

/**
 * Calculate shadow factor for a fragment position using a specific shadow map.
 * Returns 0.0 if fully in shadow, 1.0 if fully lit.
 * Uses PCF (Percentage Closer Filtering) for soft shadow edges.
 */
float calculateShadow(int light_index, vec3 frag_pos, vec3 normal, vec3 light_dir) {
	// Optimization: Quick terrain raycast for directional lights (Sun)
	float terrainShadow = 1.0;
	if (lights[light_index].type == LIGHT_TYPE_DIRECTIONAL) {
		terrainShadow = terrainShadowCoverage(frag_pos, normal, light_dir);
		if (terrainShadow <= 0.0) {
			return terrainShadow;
		}
	}

	int shadow_index = lightShadowIndices[light_index];

	// Early out for invalid indices or when no shadow lights are active
	// This MUST return before any texture operations to avoid driver issues
	if (shadow_index < 0) {
		return terrainShadow; // No shadow for this light
	}
	if (numShadowLights <= 0) {
		return terrainShadow; // No shadow maps active at all
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
		return terrainShadow; // Index out of bounds
	}

	// Transform fragment position to light space
	vec4 frag_pos_light_space = lightSpaceMatrices[shadow_index] * vec4(frag_pos, 1.0);

	// Perspective divide (guard against division by zero)
	if (abs(frag_pos_light_space.w) < 0.0001) {
		return terrainShadow;
	}
	vec3 proj_coords = frag_pos_light_space.xyz / frag_pos_light_space.w;

	// Transform to [0,1] range for texture sampling
	proj_coords = proj_coords * 0.5 + 0.5;

	// Check if fragment is outside the shadow map frustum
	if (proj_coords.x < 0.0 || proj_coords.x > 1.0 || proj_coords.y < 0.0 || proj_coords.y > 1.0 ||
	    proj_coords.z > 1.0 || proj_coords.z < 0.0) {
		return terrainShadow; // Outside shadow frustum, fully lit
	}

	// Current depth from light's perspective
	float current_depth = proj_coords.z;

	// Improved bias calculation to prevent shadow acne while keeping shadows connected to geometry
	// The key insight: larger cascades have lower resolution (larger texels), so need larger bias
	// But the bias should NOT be so large that shadows appear "floating" above terrain
	float slope_factor = max(1.0 - dot(normal, light_dir), 0.0); // 0 when facing light, 1 when perpendicular

	// Base bias: very small for direct facing surfaces
	float base_bias = 0.0001;

	// Slope bias: increases for steep angles relative to light
	float slope_bias = 0.001 * slope_factor;

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

	return min(terrainShadow, shadow);
}

/**
 * Evaluate Spherical Harmonics irradiance for a given normal and set of coefficients.
 */
vec3 evalSHIrradianceFromCoeffs(vec3 n, vec4 coeffs[9]) {
	float c1 = 0.282095;
	float c2 = 0.488603;
	float c3 = 1.092548;
	float c4 = 0.315392;
	float c5 = 0.546274;

	float a0 = 3.141593;
	float a1 = 2.094395;
	float a2 = 0.785398;

	vec3 res = vec3(0.0);
	res += a0 * c1 * coeffs[0].rgb;
	res += a1 * c2 * (coeffs[1].rgb * n.y + coeffs[2].rgb * n.z + coeffs[3].rgb * n.x);
	res += a2 * c3 * (coeffs[4].rgb * n.x * n.y + coeffs[5].rgb * n.y * n.z + coeffs[7].rgb * n.x * n.z);
	res += a2 * c4 * coeffs[6].rgb * (3.0 * n.y * n.y - 1.0);
	res += a2 * c5 * coeffs[8].rgb * (n.x * n.x - n.z * n.z);
	return max(res, 0.0);
}

/**
 * Evaluate Spherical Harmonics irradiance for a given normal.
 * Uses 2nd-order SH coefficients from the Lighting UBO.
 */
vec3 evalSHIrradiance(vec3 n) {
	return evalSHIrradianceFromCoeffs(n, sh_coeffs);
}

#ifdef USE_TERRAIN_DATA
/**
 * Look up and interpolate Spherical Harmonic ambient irradiance for a fragment.
 */
vec3 getSpatialAmbientSH(vec3 worldPos, vec3 N) {
	if (u_originSize.w < 1)
		return evalSHIrradiance(N);

	float scaledChunkSize = u_terrainParams.x * u_terrainParams.y;
	// Offset by -0.5 because probes are calculated at chunk centers (0.5, 0.5)
	vec2  gridPos = worldPos.xz / scaledChunkSize - 0.5;
	vec2  fracPos = fract(gridPos);
	ivec2 chunkCoord = ivec2(floor(gridPos)) - u_originSize.xy;

	// Simple bilinear interpolation between 4 nearest chunk probes
	vec3 totalSH[9];
	for (int i = 0; i < 9; ++i)
		totalSH[i] = vec3(0.0);

	float totalWeight = 0.0;
	for (int x = 0; x <= 1; ++x) {
		for (int z = 0; z <= 1; ++z) {
			ivec2 localCoord = chunkCoord + ivec2(x, z);
			if (localCoord.x >= 0 && localCoord.x < u_originSize.z && localCoord.y >= 0 && localCoord.y < u_originSize.z) {
				// Only include this probe if it's actually registered in the current grid
				if (texelFetch(u_chunkGrid, localCoord, 0).r >= 0) {
					float weight = (x == 0 ? 1.0 - fracPos.x : fracPos.x) * (z == 0 ? 1.0 - fracPos.y : fracPos.y);

					ivec2 worldChunkCoord = localCoord + u_originSize.xy;
					ivec2 toroidalCoord = (worldChunkCoord % u_originSize.z + u_originSize.z) % u_originSize.z;
					int   idx = toroidalCoord.y * u_originSize.z + toroidalCoord.x;

					// Verify this probe is for the correct world chunk (using encoded coordinates in w)
					vec2 probeCoord = vec2(u_terrainProbes[idx].sh_coeffs[0].w, u_terrainProbes[idx].sh_coeffs[1].w);
					if (distance(probeCoord, vec2(worldChunkCoord)) < 0.1) {
						for (int i = 0; i < 9; ++i) {
							totalSH[i] += u_terrainProbes[idx].sh_coeffs[i].rgb * weight;
						}
						totalWeight += weight;
					}
				}
			}
		}
	}

	// Smooth boundary fading for environmental bounce
	// Distance from edge of the active grid in chunks
	vec2 centerOffset = abs(gridPos - (vec2(u_originSize.xy) + float(u_originSize.z) * 0.5));
	float maxDist = float(u_originSize.z) * 0.5;
	float distToEdge = maxDist - max(centerOffset.x, centerOffset.y);
	float bounceFade = clamp(smoothstep(0.0, 0.50, distToEdge), 0.125, 1.0); // Fade over last 2 chunks

	vec4 interpolatedCoeffs[9];
	if (totalWeight > 0.001) {
		for (int i = 0; i < 9; ++i) {
			interpolatedCoeffs[i] = vec4(totalSH[i] / totalWeight, 1.0);
		}
	} else {
		// No probes available, fallback to sky only
		bounceFade = 0.0;
	}

	// Combine spatially-varying environmental bounce with global sky irradiance
	vec3 environmentalIrradiance = evalSHIrradianceFromCoeffs(N, interpolatedCoeffs);
	vec3 skyIrradiance = evalSHIrradiance(N); // Global sky/ambient fallback

	// Combine spatially-varying environmental SH (sky + bounce) with global sky irradiance.
	// We blend between them because probes now capture both sky and ground bounce.
	return mix(skyIrradiance, environmentalIrradiance, clamp(totalWeight * bounceFade, 0.0, 1.0));
}

// Forward declare macro occlusion from terrain_shadows.glsl
float calculateTerrainOcclusion(vec3 worldPos, vec3 normal);
#else
vec3 getSpatialAmbientSH(vec3 worldPos, vec3 N) {
	return evalSHIrradiance(N);
}
float calculateTerrainOcclusion(vec3 worldPos, vec3 normal) {
	return 1.0;
}
#endif

/**
 * Calculate the relative luminance of a color.
 * Used for determining how much a specular highlight should contribute to fragment opacity.
 */
float get_luminance(vec3 color) {
	return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

// ============================================================================
// PBR Lighting Functions (Cook-Torrance BRDF)
// ============================================================================

struct MicrofacetParams {
	vec2 uv;
	mat2 uv_J;
	vec3 tangent;
	vec3 bitangent;
	mat3 TBN;
	float g_density;
	float g_alpha;
	float alpha;
};

// PBR intensity multiplier to compensate for energy conservation
// PBR is inherently darker than legacy Phong.
const float PBR_INTENSITY_BOOST = 1.0;

void evaluate_brdf(
    vec3 N, vec3 V, float NdotV, vec3 L, vec3 albedo, float roughness, float metallic, vec3 F0,
    vec3 radiance, float shadow, MicrofacetParams mfp, GlintContext gctx, bool useGlints, float glintIntensity,
    inout vec3 Lo, inout float spec_lum)
{
    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0) return; // Early out

    vec3 H = normalize(V + L);
    float HdotV = max(dot(H, V), 0.0);

	float NDF = DistributionGGX(N, H, roughness);
    if (useGlints) {
        mat3 invTBN = transpose(mfp.TBN);
        vec3 h_local = invTBN * H;
        float gNDF = evaluate_glint_ndf(h_local, mfp.alpha, mfp.g_alpha, mfp.uv, gctx);
        NDF = mix(NDF, gNDF, glintIntensity);
    }

    float V_term = VisibilitySmithGGXCorrelated(NdotL, NdotV, roughness);
    vec3 F = fresnelSchlickFast(HdotV, F0);
    vec3 specular = NDF * V_term * F;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    vec3 specular_radiance = specular * radiance * NdotL * shadow;

    Lo += (kD * albedo / PI) * radiance * NdotL * shadow + specular_radiance;
    spec_lum += get_luminance(specular_radiance);
}

struct LightFactors {
	bool  shadows;
	bool  microfacetGlint;
	bool  subsurface;
	bool  usePBR;
	bool  isFoliage;
	vec3  frag_pos;
	vec3  normal;
	float occlusion;
	vec3  albedo;
	float roughness;
	float metallic;
	vec2  uv;
	mat2  uv_J;
	float glintIntensity;
	float translucency;
	float specular_strength;
};

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
vec4 apply_lighting_pbr_unified(LightFactors lf, out float primaryShadow);

vec4 apply_lighting_pbr(vec3 frag_pos, vec3 normal, vec3 albedo, float roughness, float metallic, float ao, out float primaryShadow) {
	LightFactors lf;
	lf.frag_pos = frag_pos;
	lf.normal = normal;
	lf.albedo = albedo;
	lf.roughness = roughness;
	lf.metallic = metallic;
	lf.occlusion = ao;
	lf.shadows = true;
	lf.microfacetGlint = false;
	lf.usePBR = true;
	lf.uv = frag_pos.xz;
#ifdef GL_FRAGMENT_SHADER
	lf.uv_J = mat2(dFdx(lf.uv), dFdy(lf.uv));
#else
	lf.uv_J = mat2(0.01, 0.0, 0.0, 0.01);
#endif
	return apply_lighting_pbr_unified(lf, primaryShadow);
}

vec4 apply_lighting_pbr_unified(LightFactors lf, out float primaryShadow) {
	primaryShadow = 1.0;
	vec3 N = normalize(lf.normal);
	vec3 V = normalize(viewPos - lf.frag_pos);
    float NdotV = max(dot(N, V), 1e-4);

	// Calculate reflectance at normal incidence
	// For dielectrics use 0.04, for metals use albedo color
	vec3 F0 = vec3(0.04);
	F0 = mix(F0, lf.albedo, lf.metallic);

	vec3  Lo = vec3(0.0);
	float spec_lum = 0.0;

	MicrofacetParams mfp;
    GlintContext gctx;

	float g_density, g_alpha;
	if (lf.metallic > 0.1) {
		// Metals: few intense glints
		g_density = mix(1000.0, 200.0, clamp(lf.metallic, 0.0, 1.0));
		g_alpha = mix(0.01, 0.002, clamp(lf.metallic, 0.0, 1.0));
	} else {
		// Dielectrics: more glints as it gets rougher (like snow)
		g_density = mix(500.0, 4000.0, clamp(lf.roughness, 0.0, 1.0));
		g_alpha = mix(0.02, 0.01, clamp(lf.roughness, 0.0, 1.0));
	}

	mfp.alpha = max(lf.roughness * lf.roughness, 0.0001);

	mfp.g_density = g_density;
	mfp.g_alpha = g_alpha;

	mfp.uv = lf.uv;
    mfp.uv_J = lf.uv_J;

	mfp.tangent = normalize(cross(N, vec3(0, 0, 1)));
	if (abs(N.z) > 0.9) {
			mfp.tangent = normalize(cross(N, vec3(1, 0, 0)));
	}

	mfp.bitangent = cross(N, mfp.tangent);
	mfp.TBN = mat3(mfp.tangent, mfp.bitangent, N);

    if (lf.microfacetGlint) {
        gctx = prepare_glint_context(mfp.uv, mfp.uv_J, mfp.g_density, DEFAULT_PIXEL_FILTER_SIZE);
    }

	// ------------------------------------------------------------------
	// PASS 1: Global Directional Light (Sun/Moon)
	// ------------------------------------------------------------------
	for (int i = 0; i < min(2, num_lights); ++i) {
		if (lights[i].type != LIGHT_TYPE_DIRECTIONAL) {
			continue;
		}

		if (lights[i].intensity <= 0.0) {
			continue;
		}

		vec3 L;
		float base_attenuation; // Unused for directional, but needed for your function signature
		calculateLightContribution(i, lf.frag_pos, L, base_attenuation);

		// Horizon check and normal-facing check
		if (L.y <= 0.0 || dot(N, L) <= 0.0) continue;

		// Atmospheric transmittance isolated outside the loop
		float r = kEarthRadiusKM + (lf.frag_pos.y / (1000.0 * worldScale));
		vec3 atmosphereTransmittance = texture(u_transmittanceLUT, getTransmittanceUV(r, L.y)).rgb;

		float attenuation = lights[i].intensity * PBR_INTENSITY_BOOST;
		vec3 radiance = lights[i].color * attenuation * atmosphereTransmittance;

		// Cloud shadows isolated outside the loop
		float shadow = calculateShadow(i, lf.frag_pos, N, L);
		shadow *= calculateCloudShadow(i, lf.frag_pos);

		primaryShadow = min(primaryShadow, shadow);

		evaluate_brdf(N, V, NdotV, L, lf.albedo, lf.roughness, lf.metallic, F0, radiance, shadow, mfp, gctx, lf.microfacetGlint, lf.glintIntensity, Lo, spec_lum);
	}

	// ------------------------------------------------------------------
	// PASS 2: Local Lights (Point/Spot)
	// ------------------------------------------------------------------
	for (int i = 2; i < num_lights; ++i) {
		if (lights[i].intensity <= 0.0) {
			continue;
		}

		vec3 L;
		float base_attenuation;
		calculateLightContribution(i, lf.frag_pos, L, base_attenuation);

		// Quick culling before calculating attenuation or shadows
		if (dot(N, L) <= 0.0) continue;

		float attenuation = (lights[i].intensity * PBR_INTENSITY_BOOST) * base_attenuation;
		vec3 radiance = lights[i].color * attenuation;

		float shadow = calculateShadow(i, lf.frag_pos, N, L);

		evaluate_brdf(N, V, NdotV, L, lf.albedo, lf.roughness, lf.metallic, F0, radiance, shadow, mfp, gctx, lf.microfacetGlint, lf.glintIntensity, Lo, spec_lum);
	}

	// Spatially-varying SH ambient augmented with macro occlusion
	float terrainOcc = calculateTerrainOcclusion(lf.frag_pos, N);
	vec3  spatialSHAmbient = getSpatialAmbientSH(lf.frag_pos, N);

	float combinedAO = lf.occlusion * terrainOcc;
	vec3  ambientDiffuse = spatialSHAmbient * lf.albedo * combinedAO;

	// Scale down ambient overall to maintain shadow contrast and prevent "flat" look
	// ambientDiffuse *= 0.75;

	// Environment reflection approximation for glossy surfaces
	vec3 R = reflect(-V, N);

	// Fresnel at grazing angles - smooth surfaces reflect more at edges
	vec3  F0_env = mix(vec3(0.04), lf.albedo, lf.metallic);
	vec3  F_env = fresnelSchlickRoughness(NdotV, F0_env, lf.roughness);

	// Fake environment color using spatial SH
	vec3 envColor = getSpatialAmbientSH(lf.frag_pos, R);

	// Attenuate reflection by occlusion to prevent glow in caves/valleys
	envColor *= terrainOcc;

	// Environment reflection strength based on smoothness
	float smoothness = 1.0 - lf.roughness;
	float envStrength = smoothness * smoothness * 0.8;

	// Metallic surfaces should reflect the environment color tinted by albedo
	// Non-metallic surfaces reflect environment but less strongly
	vec3 ambientSpecular = F_env * envColor * envStrength * combinedAO;

	// Combine diffuse and specular ambient
	vec3 ambient = ambientDiffuse * (1.0 - lf.metallic * 0.9) + ambientSpecular;
	vec3 color = ambient + Lo;

	return vec4(color, spec_lum + get_luminance(ambientSpecular));
}

void evaluate_foliage_brdf(
    vec3 N, vec3 V, float NdotV, vec3 L, vec3 albedo, float roughness, float metallic, vec3 F0,
    vec3 radiance, float shadow, float translucency, inout vec3 Lo, inout float spec_lum)
{
    float NdotL = dot(N, L);

    // 1. FRONT-SIDE REFLECTION (Standard PBR)
    // Only calculate specular if the light is actually in front of the normal
    if (NdotL > 0.0) {
        vec3 H = normalize(V + L);
        float HdotV = max(dot(H, V), 0.0);

        float NDF = DistributionGGX(N, H, roughness);
        float V_term = VisibilitySmithGGXCorrelated(NdotL, NdotV, roughness);
        vec3  F = fresnelSchlickFast(HdotV, F0);

        vec3 specular = NDF * V_term * F;
        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

        vec3 specular_out = specular * radiance * NdotL * shadow;
        Lo += (kD * albedo / PI) * radiance * NdotL * shadow + specular_out;
        spec_lum += get_luminance(specular_out);
    }

    // 2. BACK-SIDE TRANSMISSION (Subsurface Scattering approximation)
    // If the light is behind the camera-facing normal, it "bleeds" through.
    // We use a simplified 'Wrapped' or 'Inverted' diffuse model.
    float transmissionNdotL = max(-NdotL, 0.0);
    if (transmissionNdotL > 0.0) {
        // Translucent light is generally filtered by the material color (albedo)
        // and doesn't have a specular component.
        vec3 transmission_out = (albedo / PI) * radiance * transmissionNdotL * translucency * shadow;
        Lo += transmission_out;
    }
}

vec4 apply_lighting_foliage_unified(LightFactors lf, out float primaryShadow);

vec4 apply_lighting_foliage(vec3 frag_pos, vec3 normal, vec3 albedo, float roughness, float metallic, float ao, out float primaryShadow) {
	LightFactors lf;
	lf.frag_pos = frag_pos;
	lf.normal = normal;
	lf.albedo = albedo;
	lf.roughness = roughness;
	lf.metallic = metallic;
	lf.occlusion = ao;
	lf.shadows = true;
	lf.isFoliage = true;
	lf.translucency = 0.5;
	return apply_lighting_foliage_unified(lf, primaryShadow);
}

vec4 apply_lighting_foliage_unified(LightFactors lf, out float primaryShadow) {
    primaryShadow = 1.0;
    vec3 N = normalize(lf.normal);
    vec3 V = normalize(viewPos - lf.frag_pos);
    float NdotV = max(dot(N, V), 1e-4);
    vec3 F0 = mix(vec3(0.04), lf.albedo, lf.metallic);

    vec3 Lo = vec3(0.0);
    float spec_lum = 0.0;

    // PASS 1: Directional Light (with Atmosphere & Clouds)
	for (int i = 0; i < min(2, num_lights); ++i) {
		if (lights[i].type != LIGHT_TYPE_DIRECTIONAL) {
			continue;
		}
		if (lights[i].intensity <= 0.0) {
			continue;
		}

        vec3 L; float atten;
        calculateLightContribution(i, lf.frag_pos, L, atten);

		// Horizon check
		if (L.y <= 0.0) continue;

        float r = kEarthRadiusKM + (lf.frag_pos.y / (1000.0 * worldScale));
        vec3 atmosphereTransmittance = texture(u_transmittanceLUT, getTransmittanceUV(r, L.y)).rgb;
        vec3 radiance = lights[i].color * (lights[i].intensity * PBR_INTENSITY_BOOST) * atmosphereTransmittance;

        float shadow = min(calculateShadow(i, lf.frag_pos, N, L), calculateCloudShadow(i, lf.frag_pos));

		primaryShadow = min(primaryShadow, shadow);

        evaluate_foliage_brdf(N, V, NdotV, L, lf.albedo, lf.roughness, lf.metallic, F0, radiance, shadow, lf.translucency, Lo, spec_lum);
    }

    // PASS 2: Local Lights
    for (int i = min(2, num_lights); i < num_lights; ++i) {
		if (lights[i].intensity <= 0.0) {
			continue;
		}

        vec3 L; float base_atten;
        calculateLightContribution(i, lf.frag_pos, L, base_atten);

        // We only skip if the light is perpendicular (rare)
        if (abs(dot(N, L)) < 0.0001) continue;

        vec3 radiance = lights[i].color * (lights[i].intensity * PBR_INTENSITY_BOOST) * base_atten;
        float shadow = calculateShadow(i, lf.frag_pos, N, L);

        evaluate_foliage_brdf(N, V, NdotV, L, lf.albedo, lf.roughness, lf.metallic, F0, radiance, shadow, lf.translucency, Lo, spec_lum);
    }

    // Standard Ambient (using your existing SH logic)
    float terrainOcc = calculateTerrainOcclusion(lf.frag_pos, N);
    vec3 ambient = (getSpatialAmbientSH(lf.frag_pos, N) * lf.albedo) * (lf.occlusion * terrainOcc);

    return vec4(ambient + Lo, spec_lum);
}

/**
 * PBR lighting without shadows - for shaders that don't need shadow calculations.
 * Supports all light types (point, directional, spot).
 * Returns vec4(color.rgb, specular_luminance).
 */
vec4 apply_lighting_pbr_no_shadows(vec3 frag_pos, vec3 normal, vec3 albedo, float roughness, float metallic, float ao, out float primaryShadow) {
	LightFactors lf;
	lf.frag_pos = frag_pos;
	lf.normal = normal;
	lf.albedo = albedo;
	lf.roughness = roughness;
	lf.metallic = metallic;
	lf.occlusion = ao;
	lf.shadows = false;
	lf.microfacetGlint = false;
	lf.usePBR = true;
	lf.uv = frag_pos.xz;
#ifdef GL_FRAGMENT_SHADER
	lf.uv_J = mat2(dFdx(lf.uv), dFdy(lf.uv));
#else
	lf.uv_J = mat2(0.01, 0.0, 0.0, 0.01);
#endif
	return apply_lighting_pbr_unified(lf, primaryShadow);
}

vec4 apply_lighting_pbr_no_shadows_legacy(vec3 frag_pos, vec3 normal, vec3 albedo, float roughness, float metallic, float ao, out float primaryShadow) {
	primaryShadow = 1.0;
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
		vec3  atmosphereTransmittance = vec3(1.0);

		if (lights[i].type == LIGHT_TYPE_DIRECTIONAL) {
			attenuation = lights[i].intensity * PBR_INTENSITY_BOOST;

			// Apply atmospheric attenuation
			float r = kEarthRadiusKM + (frag_pos.y / (1000.0 * worldScale));
			float mu = L.y;
			atmosphereTransmittance = texture(u_transmittanceLUT, getTransmittanceUV(r, mu)).rgb;
		} else {
			attenuation = (lights[i].intensity * PBR_INTENSITY_BOOST) * base_attenuation;
		}
		vec3 radiance = lights[i].color * attenuation * atmosphereTransmittance;

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

		// Apply cloud shadow for directional lights
		float shadow = 1.0;
		if (lights[i].type == LIGHT_TYPE_DIRECTIONAL) {
			shadow *= calculateCloudShadow(i, frag_pos);
		}

		if (i == 0)
			primaryShadow = shadow;

		vec3 specular_radiance = specular * radiance * NdotL * shadow;
		Lo += (kD * albedo / PI) * radiance * NdotL * shadow + specular_radiance;
		spec_lum += get_luminance(specular_radiance);
	}

	// Spatially-varying SH ambient augmented with macro occlusion
	float terrainOcc = calculateTerrainOcclusion(frag_pos, N);
	vec3  spatialSHAmbient = getSpatialAmbientSH(frag_pos, N);

	float combinedAO = ao * terrainOcc;
	vec3  ambientDiffuse = spatialSHAmbient * albedo * combinedAO;

	// Scale down ambient overall to maintain shadow contrast
	ambientDiffuse *= 0.75;

	vec3 R = reflect(-V, N);

	vec3  F0_env = mix(vec3(0.04), albedo, metallic);
	float NdotV = max(dot(N, V), 0.0);
	vec3  F_env = fresnelSchlickRoughness(NdotV, F0_env, roughness);

	// Fake environment color using spatial SH
	vec3 envColor = getSpatialAmbientSH(frag_pos, R);

	// Attenuate reflection by occlusion to prevent glow in caves/valleys
	envColor *= terrainOcc;

	float smoothness = 1.0 - roughness;
	float envStrength = smoothness * smoothness * 0.8;
	vec3  ambientSpecular = F_env * envColor * envStrength * combinedAO;
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
vec4 apply_lighting(vec3 frag_pos, vec3 normal, vec3 albedo, float specular_strength, out float primaryShadow) {
	primaryShadow = 1.0;
	vec3  result = ambient_light * albedo;
	float spec_lum = 0.0;

	for (int i = 0; i < num_lights; ++i) {
		vec3  light_dir;
		float attenuation;
		calculateLightContribution(i, frag_pos, light_dir, attenuation);

		// Calculate shadow factor for this light with slope-scaled bias
		float shadow = calculateShadow(i, frag_pos, normal, light_dir);

		// Atmospheric attenuation for directional light
		vec3 atmosphereTransmittance = vec3(1.0);
		if (lights[i].type == LIGHT_TYPE_DIRECTIONAL) {
			float r = kEarthRadiusKM + (frag_pos.y / (1000.0 * worldScale));
			float mu = light_dir.y;
			atmosphereTransmittance = texture(u_transmittanceLUT, getTransmittanceUV(r, mu)).rgb;

			// Apply cloud shadow
			shadow *= calculateCloudShadow(i, frag_pos);
		}

		if (i == 0)
			primaryShadow = shadow;

		// Diffuse
		float diff = max(dot(normal, light_dir), 0.0);
		vec3  diffuse = lights[i].color * atmosphereTransmittance * diff * albedo;

		// Specular (Blinn-Phong)
		vec3  view_dir = normalize(viewPos - frag_pos);
		vec3  reflect_dir = reflect(-light_dir, normal);
		float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32);
		vec3  specular_contribution = lights[i].color * atmosphereTransmittance * spec * specular_strength *
			lights[i].intensity * shadow * attenuation;

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
vec4 apply_lighting_no_shadows(vec3 frag_pos, vec3 normal, vec3 albedo, float specular_strength, out float primaryShadow) {
	primaryShadow = 1.0;
	vec3  result = ambient_light * albedo;
	float spec_lum = 0.0;

	for (int i = 0; i < num_lights; ++i) {
		vec3  light_dir;
		float attenuation;
		calculateLightContribution(i, frag_pos, light_dir, attenuation);

		// Atmospheric attenuation for directional light
		vec3  atmosphereTransmittance = vec3(1.0);
		if (lights[i].type == LIGHT_TYPE_DIRECTIONAL) {
			float r = kEarthRadiusKM + (frag_pos.y / (1000.0 * worldScale));
			float mu = light_dir.y;
			atmosphereTransmittance = texture(u_transmittanceLUT, getTransmittanceUV(r, mu)).rgb;
		}

		// Diffuse
		float diff = max(dot(normal, light_dir), 0.0);
		vec3  diffuse = lights[i].color * atmosphereTransmittance * diff * albedo;

		// Specular
		vec3  view_dir = normalize(viewPos - frag_pos);
		vec3  reflect_dir = reflect(-light_dir, normal);
		float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32);
		vec3  specular_contribution = lights[i].color * atmosphereTransmittance * spec * specular_strength *
			lights[i].intensity * attenuation;

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
	float iridescence_strength,
	out float primaryShadow
) {
	primaryShadow = 1.0;
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
		vec3  atmosphereTransmittance = vec3(1.0);

		if (lights[i].type == LIGHT_TYPE_DIRECTIONAL) {
			attenuation = lights[i].intensity * PBR_INTENSITY_BOOST;

			// Apply atmospheric attenuation
			float r = kEarthRadiusKM + (frag_pos.y / (1000.0 * worldScale));
			float mu = L.y;
			atmosphereTransmittance = texture(u_transmittanceLUT, getTransmittanceUV(r, mu)).rgb;
		} else {
			attenuation = (lights[i].intensity * PBR_INTENSITY_BOOST) * base_attenuation;
		}
		vec3 radiance = lights[i].color * attenuation * atmosphereTransmittance;

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
	float emissive_coverage,
	out float primaryShadow
) {
	primaryShadow = 1.0;
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
		lit_surface = apply_lighting_no_shadows(frag_pos, normal, base_albedo, 0.5, primaryShadow);
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
 * @param emissive_color The glow color of the object
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
	float emissive_mask,
	out float primaryShadow
) {
	primaryShadow = 1.0;
	vec3 N = normalize(normal);
	vec3 V = normalize(viewPos - frag_pos);

	// Emissive component
	vec3  emission = emissive_color * emissive_intensity;
	float fresnel = pow(1.0 - max(dot(N, V), 0.0), 3.0);
	emission += emissive_color * fresnel * emissive_intensity * 0.3;

	// PBR lit component for non-emissive parts
	vec4 pbr_lit = apply_lighting_pbr_no_shadows(frag_pos, normal, base_albedo, roughness, metallic, 1.0, primaryShadow);

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


struct LightOutput {
	vec4 lighting;
	float primaryShadow;
};

LightOutput apply_lighting(LightFactors lf) {
    float shadow = 1.0;
    vec4 light = vec4(0.0);

    if (lf.usePBR) {
        if (lf.isFoliage) {
            light = apply_lighting_foliage_unified(lf, shadow);
        } else {
            light = apply_lighting_pbr_unified(lf, shadow);
        }
    } else {
        if (lf.shadows) {
            light = apply_lighting(lf.frag_pos, lf.normal, lf.albedo, lf.specular_strength, shadow);
        } else {
            light = apply_lighting_no_shadows(lf.frag_pos, lf.normal, lf.albedo, lf.specular_strength, shadow);
        }
    }

	LightOutput lo;
	lo.primaryShadow = shadow;
	lo.lighting = light;

	return lo;
}


#endif // HELPERS_LIGHTING_GLSL
