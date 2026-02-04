#version 420 core

out vec4 FragColor;

in vec2 TexCoords;

#include "helpers/lighting.glsl"
#include "helpers/atmosphere.glsl"
#include "helpers/noise.glsl"

uniform mat4 invProjection;
uniform mat4 invView;

// A simple 3D hash function (returns pseudo-random vec3 between 0 and 1)
vec3 hash33(vec3 p) {
	p = fract(p * vec3(443.897, 441.423, 437.195));
	p += dot(p, p.yxz + 19.19);
	return fract((p.xxy + p.yxx) * p.zyx);
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
	float brightness = abs(sin(time / 2.0 + star_pos.x * 100.0));

	// 4. Distance check: Are we looking at the star?
	// Centering the star in the cell (0.5) + jitter (star_pos - 0.5)
	vec3  center = vec3(0.5) + (star_pos - 0.5) * 0.8;
	float dist = length(local_uv - center);

	float radius = 0.05 * brightness; // Modulate radius by brightness for "glow"
	return smoothstep(radius, radius * 0.5, dist);
}

float fbm(vec3 p) {
	float value = 0.0;
	float amplitude = 0.5;
	for (int i = 0; i < 4; i++) {
		value += amplitude * snoise(p.xy); // snoise in noise.glsl is 2D
		p *= 2.0;
		amplitude *= 0.5;
	}
	return value;
}

void main() {
	// 1. Reconstruct World Ray
	vec4 clip = vec4(TexCoords * 2.0 - 1.0, 1.0, 1.0);
	vec4 view_ray = invProjection * clip;
	vec3 world_ray = (invView * vec4(view_ray.xy, -1.0, 0.0)).xyz;
	world_ray = normalize(world_ray);

	vec3 final_sky_color = vec3(0.0);

	// 2. Atmospheric Scattering from all directional lights
	for (int i = 0; i < num_lights; i++) {
		if (lights[i].type == LIGHT_TYPE_DIRECTIONAL) {
			vec3 sun_dir = normalize(-lights[i].direction);
			final_sky_color += calculateSkyColor(world_ray, sun_dir, lights[i].color * lights[i].intensity);
		}
	}

	// Base ambient sky if no directional lights
	float y = world_ray.y;
	vec3  top_sky_color = vec4(0.05, 0.1, 0.3, 1.0).rgb;
	vec3  horizon_color = vec3(0.2, 0.4, 0.8);
	vec3  sky_gradient = mix(horizon_color, top_sky_color, smoothstep(0.0, 0.6, y));
	final_sky_color = max(final_sky_color, sky_gradient * 0.2);

	// --- 3. Nebula/Haze Layer (Domain Warping + FBM) ---
	vec3  p = world_ray * 4.0;
	float n1 = snoise(p.xy + time * 0.05);
	float n2 = snoise(p.zx - time * 0.03);
	float nebula_noise = mix(n1, n2, 0.5) * 0.5 + 0.5;

	vec3 nebula_palette = mix(vec3(0.0, 0.1, 0.4), vec3(0.4, 0.1, 0.5), nebula_noise);
	float nebula_strength = smoothstep(0.1, 0.8, y);
	final_sky_color = mix(final_sky_color, nebula_palette, nebula_strength * 0.3 * nebula_noise);

	// --- 4. Star Field Layer (Additive Blend) ---
	float stars = starLayer(world_ray);
	// Only show stars where sky is dark
	float star_visibility = smoothstep(0.5, 1.0, 1.0 - length(final_sky_color));
	final_sky_color += stars * vec3(1.0, 0.9, 0.8) * star_visibility;

	FragColor = vec4(final_sky_color, 1.0);
}
