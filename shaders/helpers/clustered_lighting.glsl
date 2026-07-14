#ifndef HELPERS_CLUSTERED_LIGHTING_GLSL
#define HELPERS_CLUSTERED_LIGHTING_GLSL

#include "../types/clustered_lighting.glsl"

/**
 * Computes the 1D cluster index for a given world-space position.
 */
uint getClusterIndex(vec3 frag_pos) {
	// Transform world position to view space
	vec4 view_pos = view * vec4(frag_pos, 1.0);
	float z_val = -view_pos.z; // OpenGL has camera facing -Z in view space

	float z_near = zNear;
	float z_far = zFar;

	// Calculate depth slice logarithmically
	int z_slice = int(clamp(log(max(z_val, 0.001) / z_near) / log(z_far / z_near) * 24.0, 0.0, 23.0));

	// Project view-space position to clip space, then compute screen coordinates factor
	vec4 clip_pos = projection * view_pos;
	vec3 ndc_pos = clip_pos.xyz / max(0.0001, clip_pos.w);
	vec2 screen_uv = ndc_pos.xy * 0.5 + 0.5;

	int x_slice = int(clamp(screen_uv.x * 16.0, 0.0, 15.0));
	int y_slice = int(clamp(screen_uv.y * 9.0, 0.0, 8.0));

	return uint(x_slice + y_slice * 16 + z_slice * 16 * 9);
}

/**
 * High-level GLSL helper to evaluate clustered local light contribution with normal.
 */
vec3 evaluateClusteredLightContribution(vec3 frag_pos, vec3 normal) {
	vec3 n = normalize(normal);
	vec3 total_light = vec3(0.0);

	// Evaluate global lights loop first (index 3456)
	Cluster global_cluster = clusters[3456];
	for (uint i = 0; i < global_cluster.count; ++i) {
		uint light_index = global_cluster.lightIndices[i];
		if (lights[light_index].intensity <= 0.0) continue;

		vec3 light_pos = lights[light_index].position;
		if ((lights[light_index].flags & LIGHT_FLAG_CAMERA_RELATIVE) != 0) {
			light_pos += viewPos;
		}

		vec3 L;
		float attenuation = 1.0;

		if (lights[light_index].type == 0) { // POINT
			L = normalize(light_pos - frag_pos);
			float distance = length(light_pos - frag_pos);
			attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
		} else if (lights[light_index].type == 2) { // SPOT
			L = normalize(light_pos - frag_pos);
			float distance = length(light_pos - frag_pos);
			attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);

			float theta = dot(L, normalize(-lights[light_index].direction));
			float epsilon = lights[light_index].inner_cutoff - lights[light_index].outer_cutoff;
			float angular_intensity = clamp((theta - lights[light_index].outer_cutoff) / epsilon, 0.0, 1.0);
			attenuation *= angular_intensity;
		} else if (lights[light_index].type == 3) { // EMISSIVE
			L = normalize(light_pos - frag_pos);
			float distance = length(light_pos - frag_pos);
			float emissive_radius = lights[light_index].inner_cutoff;
			float effective_dist = max(distance - emissive_radius * 0.5, 0.0);
			attenuation = 1.0 / (1.0 + 0.09 * effective_dist + 0.032 * effective_dist * effective_dist);
			float proximity_boost = smoothstep(emissive_radius * 2.0, 0.0, distance);
			attenuation = mix(attenuation, 1.0, proximity_boost * 0.5);
		} else if (lights[light_index].type == 4) { // FLASH
			L = normalize(light_pos - frag_pos);
			float distance = length(light_pos - frag_pos);
			float flash_radius = lights[light_index].inner_cutoff;
			float falloff_exp = lights[light_index].outer_cutoff;
			float norm_dist = distance / max(flash_radius, 0.001);
			attenuation = 1.0 / pow(1.0 + norm_dist, falloff_exp);
			attenuation *= smoothstep(2.0, 1.5, norm_dist);
		} else {
			continue;
		}

		float NdotL = max(dot(n, L), 0.0);
		total_light += lights[light_index].color * lights[light_index].intensity * attenuation * NdotL;
	}

	uint cluster_index = getClusterIndex(frag_pos);
	Cluster cluster = clusters[cluster_index];

	for (uint i = 0; i < cluster.count; ++i) {
		uint light_index = cluster.lightIndices[i];
		if (lights[light_index].intensity <= 0.0) continue;

		vec3 light_pos = lights[light_index].position;
		if ((lights[light_index].flags & LIGHT_FLAG_CAMERA_RELATIVE) != 0) {
			light_pos += viewPos;
		}

		vec3 L;
		float attenuation = 1.0;

		if (lights[light_index].type == 0) { // POINT
			L = normalize(light_pos - frag_pos);
			float distance = length(light_pos - frag_pos);
			attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
		} else if (lights[light_index].type == 2) { // SPOT
			L = normalize(light_pos - frag_pos);
			float distance = length(light_pos - frag_pos);
			attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);

			float theta = dot(L, normalize(-lights[light_index].direction));
			float epsilon = lights[light_index].inner_cutoff - lights[light_index].outer_cutoff;
			float angular_intensity = clamp((theta - lights[light_index].outer_cutoff) / epsilon, 0.0, 1.0);
			attenuation *= angular_intensity;
		} else if (lights[light_index].type == 3) { // EMISSIVE
			L = normalize(light_pos - frag_pos);
			float distance = length(light_pos - frag_pos);
			float emissive_radius = lights[light_index].inner_cutoff;
			float effective_dist = max(distance - emissive_radius * 0.5, 0.0);
			attenuation = 1.0 / (1.0 + 0.09 * effective_dist + 0.032 * effective_dist * effective_dist);
			float proximity_boost = smoothstep(emissive_radius * 2.0, 0.0, distance);
			attenuation = mix(attenuation, 1.0, proximity_boost * 0.5);
		} else if (lights[light_index].type == 4) { // FLASH
			L = normalize(light_pos - frag_pos);
			float distance = length(light_pos - frag_pos);
			float flash_radius = lights[light_index].inner_cutoff;
			float falloff_exp = lights[light_index].outer_cutoff;
			float norm_dist = distance / max(flash_radius, 0.001);
			attenuation = 1.0 / pow(1.0 + norm_dist, falloff_exp);
			attenuation *= smoothstep(2.0, 1.5, norm_dist);
		} else {
			continue;
		}

		float NdotL = max(dot(n, L), 0.0);
		total_light += lights[light_index].color * lights[light_index].intensity * attenuation * NdotL;
	}

	return total_light;
}

/**
 * High-level GLSL helper to evaluate clustered local light contribution without normals (isotropic/ambient particles/lightning).
 */
vec3 evaluateClusteredLightContributionSimple(vec3 frag_pos) {
	vec3 total_light = vec3(0.0);

	// Evaluate global lights loop first (index 3456)
	Cluster global_cluster = clusters[3456];
	for (uint i = 0; i < global_cluster.count; ++i) {
		uint light_index = global_cluster.lightIndices[i];
		if (lights[light_index].intensity <= 0.0) continue;

		vec3 light_pos = lights[light_index].position;
		if ((lights[light_index].flags & LIGHT_FLAG_CAMERA_RELATIVE) != 0) {
			light_pos += viewPos;
		}

		float attenuation = 1.0;

		if (lights[light_index].type == 0) { // POINT
			float distance = length(light_pos - frag_pos);
			attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
		} else if (lights[light_index].type == 2) { // SPOT
			vec3 L = normalize(light_pos - frag_pos);
			float distance = length(light_pos - frag_pos);
			attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);

			float theta = dot(L, normalize(-lights[light_index].direction));
			float epsilon = lights[light_index].inner_cutoff - lights[light_index].outer_cutoff;
			float angular_intensity = clamp((theta - lights[light_index].outer_cutoff) / epsilon, 0.0, 1.0);
			attenuation *= angular_intensity;
		} else if (lights[light_index].type == 3) { // EMISSIVE
			float distance = length(light_pos - frag_pos);
			float emissive_radius = lights[light_index].inner_cutoff;
			float effective_dist = max(distance - emissive_radius * 0.5, 0.0);
			attenuation = 1.0 / (1.0 + 0.09 * effective_dist + 0.032 * effective_dist * effective_dist);
			float proximity_boost = smoothstep(emissive_radius * 2.0, 0.0, distance);
			attenuation = mix(attenuation, 1.0, proximity_boost * 0.5);
		} else if (lights[light_index].type == 4) { // FLASH
			float distance = length(light_pos - frag_pos);
			float flash_radius = lights[light_index].inner_cutoff;
			float falloff_exp = lights[light_index].outer_cutoff;
			float norm_dist = distance / max(flash_radius, 0.001);
			attenuation = 1.0 / pow(1.0 + norm_dist, falloff_exp);
			attenuation *= smoothstep(2.0, 1.5, norm_dist);
		} else {
			continue;
		}

		total_light += lights[light_index].color * lights[light_index].intensity * attenuation;
	}

	uint cluster_index = getClusterIndex(frag_pos);
	Cluster cluster = clusters[cluster_index];

	for (uint i = 0; i < cluster.count; ++i) {
		uint light_index = cluster.lightIndices[i];
		if (lights[light_index].intensity <= 0.0) continue;

		vec3 light_pos = lights[light_index].position;
		if ((lights[light_index].flags & LIGHT_FLAG_CAMERA_RELATIVE) != 0) {
			light_pos += viewPos;
		}

		float attenuation = 1.0;

		if (lights[light_index].type == 0) { // POINT
			float distance = length(light_pos - frag_pos);
			attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
		} else if (lights[light_index].type == 2) { // SPOT
			vec3 L = normalize(light_pos - frag_pos);
			float distance = length(light_pos - frag_pos);
			attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);

			float theta = dot(L, normalize(-lights[light_index].direction));
			float epsilon = lights[light_index].inner_cutoff - lights[light_index].outer_cutoff;
			float angular_intensity = clamp((theta - lights[light_index].outer_cutoff) / epsilon, 0.0, 1.0);
			attenuation *= angular_intensity;
		} else if (lights[light_index].type == 3) { // EMISSIVE
			float distance = length(light_pos - frag_pos);
			float emissive_radius = lights[light_index].inner_cutoff;
			float effective_dist = max(distance - emissive_radius * 0.5, 0.0);
			attenuation = 1.0 / (1.0 + 0.09 * effective_dist + 0.032 * effective_dist * effective_dist);
			float proximity_boost = smoothstep(emissive_radius * 2.0, 0.0, distance);
			attenuation = mix(attenuation, 1.0, proximity_boost * 0.5);
		} else if (lights[light_index].type == 4) { // FLASH
			float distance = length(light_pos - frag_pos);
			float flash_radius = lights[light_index].inner_cutoff;
			float falloff_exp = lights[light_index].outer_cutoff;
			float norm_dist = distance / max(flash_radius, 0.001);
			attenuation = 1.0 / pow(1.0 + norm_dist, falloff_exp);
			attenuation *= smoothstep(2.0, 1.5, norm_dist);
		} else {
			continue;
		}

		total_light += lights[light_index].color * lights[light_index].intensity * attenuation;
	}

	return total_light;
}

#endif
