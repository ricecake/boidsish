#version 330 core

out vec4 FragColor;

in vec2 TexCoords;

#include "helpers/lighting.glsl"
#include "helpers/noise.glsl"

uniform mat4 invProjection;
uniform mat4 invView;

// A simple 3D hash function (returns pseudo-random vec3 between 0 and 1)
vec3 hash33(vec3 p) {
	p = fract(p * vec3(443.897, 441.423, 437.195));
	p += dot(p, p.yxz + 19.19);
	return fract((p.xxy + p.yxx) * p.zyx);
}

// A robust 1D hash function (Returns float between 0.0 and 1.0)
float hash11(float p) {
	p = fract(p * 0.1031);
	p *= p + 33.33;
	p = fract(p);
	return fract(p * 0.5);
}

// Function to convert 3D integer coordinates to a unique float seed
float getSeed(vec3 p) {
	return p.x * 123.0 + p.y * 456.0 + p.z * 789.0;
}

float starLayer(vec3 dir) {
	// 1. Tiling: Scale the direction to create a grid
	float scale = 100.0;
	vec3  id = floor(dir * scale);
	vec3  local_uv = fract(dir * scale); // 0.0 to 1.0 inside the cell

	// 2. Random Position: Where is the star in this cell?
	vec3 star_pos = hash33(id);

	// 3. Animation: Twinkle logic
	// use the star's unique ID to give it a unique offset so they don't pulse in sync
	float brightness = abs(sin(time / 2 + star_pos.x * 100));

	// 4. Distance check: Are we looking at the star?
	// Centering the star in the cell (0.5) + jitter (star_pos - 0.5)
	vec3  center = vec3(0.5) + (star_pos - 0.5) * 0.8;
	float dist = length(local_uv - center);

	float radius = 0.05 * brightness; // Modulate radius by brightness for "glow"
	return smoothstep(radius, radius * 0.5, dist);
}

void main() {
	vec3 u_sunset_orange = vec3(0.9, 0.5, 0.2); // Orangey-red
	vec3 u_horizon_color = vec3(0.2, 0.4, 0.8); // Gentle blue
	vec3 u_night_color = vec3(0.05, 0.1, 0.3);  // Dark blue
	vec3 top_sky_color = vec3(0.05, 0.1, 0.3); // Dark blue

	// 1. Reconstruct World Ray
	// Convert screen coordinates to a world-space direction vector
	vec4 clip = vec4(TexCoords * 2.0 - 1.0, 1.0, 1.0);
	vec4 view_ray = invProjection * clip;
	vec3 world_ray = (invView * vec4(view_ray.xy, -1.0, 0.0)).xyz;
	world_ray = normalize(world_ray);

	// 2. The Sun/Light Source
	vec3  sun_dir = normalize(vec3(0.0, 0.1, -1.0)); // Near horizon
	float sun_alignment = max(0.0, dot(world_ray, sun_dir));

	// 3. Calculate the "Sunset Glow"
	// Use an exponential falloff based on the world_ray.y
	// This creates a "thick" atmosphere near y=0
	float atmosphere_thickness = exp(-abs(world_ray.y) * 15.0);

	// 4. Directional Masking (Mie Scattering Approximation)
	// This concentrates the orange glow around the sun
	float glow_mask = pow(sun_alignment, 4.0) * atmosphere_thickness;

	// 5. Adding "Sunset Wisps" using Noise
	// We sample noise based on the ray direction, but squash it horizontally
	float sunset_noise = snoise(vec3(world_ray.x * 10.0, world_ray.y * 40.0, world_ray.z * 10.0));
	float final_glow = glow_mask * (1.0 + sunset_noise * 0.4);

	// 6. Final Mixing
	vec3 sky_base = mix(u_night_color, u_horizon_color, atmosphere_thickness * 0.5);
	vec3 sunset_final = mix(sky_base, u_sunset_orange, final_glow);

	float top_mix = smoothstep(0.1, 0.6, world_ray.y);
	vec3  final_color = mix(sunset_final, top_sky_color, top_mix);

	// --- 7. Nebula/Haze Layer (Domain Warping + FBM) ---
	vec3  p = world_ray * 4.0;
	vec3  warp_offset = vec3(fbm(p + time * 0.05));
	float nebula_noise = fbm(p + warp_offset * 0.5);

	// Map noise to a color palette (e.g., magenta and cyan for cosmic clouds)
	vec3 nebula_palette = mix(vec3(0.0, 0.1, 0.4), vec3(0.8, 0.2, 0.7), nebula_noise);

	// Blend the nebula color into the base sky, mostly visible in the dark top_sky area
	float nebula_strength = mix(0.2, 0.6, top_mix); // Only fade it in when sky is dark
	final_color = mix(final_color, nebula_palette * 1.5, nebula_strength * 0.4);

	// --- 8. Star Field Layer (Additive Blend) ---
	float stars = starLayer(world_ray);
	final_color += stars * vec3(1.0, 0.9, 0.8);

	// --- 9. "Eye of God" Light Source ---
	if (num_lights > 0) {
		vec3  light_dir = normalize(lights[0].position - viewPos);
		float alignment = dot(world_ray, light_dir);

		// Create a soft, bright spot where the alignment is high
		float eye_spot = smoothstep(0.99, 1.0, alignment);
		vec3  eye_color = lights[0].color * 0.08; // Bright, warm color

		// Create a darker "iris" for the eye effect
		float iris = smoothstep(0.995, 0.998, alignment) - smoothstep(0.998, 1.0, alignment);
		vec3  iris_color = lights[0].color * 0.5;

		final_color += eye_color * eye_spot * nebula_strength;
		final_color += iris_color * iris * nebula_strength;
	}

	FragColor = vec4(final_color, 1.0);
}
