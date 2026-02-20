#version 330 core

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec2 Velocity;
layout(location = 2) out vec3 WorldNormal;
layout(location = 3) out vec4 MaterialData;

in vec2 TexCoords;

#include "helpers/lighting.glsl"

uniform mat4 invProjection;
uniform mat4 invView;

vec3 mod289(vec3 x) {
	return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 mod289(vec4 x) {
	return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 permute(vec4 x) {
	return mod289(((x * 34.0) + 1.0) * x);
}

vec4 taylorInvSqrt(vec4 r) {
	return 1.79284291400159 - 0.85373472095314 * r;
}

// vec4 permute(vec4 x){return mod(((x*34.0)+1.0)*x, 289.0);}
vec2 fade(vec2 t) {
	return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

float snoise(vec2 P) {
	vec4 Pi = floor(P.xyxy) + vec4(0.0, 0.0, 1.0, 1.0);
	vec4 Pf = fract(P.xyxy) - vec4(0.0, 0.0, 1.0, 1.0);
	Pi = mod(Pi, 289.0); // Keep values in range
	vec4 ix = Pi.xzxz;
	vec4 iy = Pi.yyww;
	vec4 fx = Pf.xzxz;
	vec4 fy = Pf.yyww;

	vec4 i = permute(permute(ix) + iy);

	vec4 gx = fract(i * (1.0 / 41.0)) * 2.0 - 1.0;
	vec4 gy = abs(gx) - 0.5;
	vec4 tx = floor(gx + 0.5);
	gx = gx - tx;

	vec2 g00 = vec2(gx.x, gy.x);
	vec2 g10 = vec2(gx.y, gy.y);
	vec2 g01 = vec2(gx.z, gy.z);
	vec2 g11 = vec2(gx.w, gy.w);

	vec4 norm = 1.79284291400159 - 0.85373472095314 * vec4(dot(g00, g00), dot(g10, g10), dot(g01, g01), dot(g11, g11));
	g00 *= norm.x;
	g10 *= norm.y;
	g01 *= norm.z;
	g11 *= norm.w;

	float n00 = dot(g00, vec2(fx.x, fy.x));
	float n10 = dot(g10, vec2(fx.y, fy.y));
	float n01 = dot(g01, vec2(fx.z, fy.z));
	float n11 = dot(g11, vec2(fx.w, fy.w));

	vec2 t = fade(Pf.xy);
	return 2.3 * mix(mix(n00, n10, t.x), mix(n01, n11, t.x), t.y);
}

float snoise(vec3 v) {
	const vec2 C = vec2(1.0 / 6.0, 1.0 / 3.0);
	const vec4 D = vec4(0.0, 0.5, 1.0, 2.0);

	// First corner
	vec3 i = floor(v + dot(v, C.yyy));
	vec3 x0 = v - i + dot(i, C.xxx);

	// Other corners
	vec3 g = step(x0.yzx, x0.xyz);
	vec3 l = 1.0 - g;
	vec3 i1 = min(g.xyz, l.zxy);
	vec3 i2 = max(g.xyz, l.zxy);

	vec3 x1 = x0 - i1 + C.xxx;
	vec3 x2 = x0 - i2 + C.yyy;
	vec3 x3 = x0 - D.yyy;

	// Permutations
	i = mod289(i);
	vec4 p = permute(
		permute(permute(i.z + vec4(0.0, i1.z, i2.z, 1.0)) + i.y + vec4(0.0, i1.y, i2.y, 1.0)) + i.x +
		vec4(0.0, i1.x, i2.x, 1.0)
	);

	float n_ = 0.142857142857;
	vec3  ns = n_ * D.wyz - D.xzx;

	vec4 j = p - 49.0 * floor(p * ns.z * ns.z);

	vec4 x_ = floor(j * ns.z);
	vec4 y_ = floor(j - 7.0 * x_);

	vec4 x = x_ * ns.x + ns.yyyy;
	vec4 y = y_ * ns.x + ns.yyyy;
	vec4 h = 1.0 - abs(x) - abs(y);

	vec4 b0 = vec4(x.xy, y.xy);
	vec4 b1 = vec4(x.zw, y.zw);

	vec4 s0 = floor(b0) * 2.0 + 1.0;
	vec4 s1 = floor(b1) * 2.0 + 1.0;
	vec4 sh = -step(h, vec4(0.0));

	vec4 a0 = b0.xzyw + s0.xzyw * sh.xxyy;
	vec4 a1 = b1.xzyw + s1.xzyw * sh.zzww;

	vec3 p0 = vec3(a0.xy, h.x);
	vec3 p1 = vec3(a0.zw, h.y);
	vec3 p2 = vec3(a1.xy, h.z);
	vec3 p3 = vec3(a1.zw, h.w);

	// Normalise gradients
	vec4 norm = taylorInvSqrt(vec4(dot(p0, p0), dot(p1, p1), dot(p2, p2), dot(p3, p3)));
	p0 *= norm.x;
	p1 *= norm.y;
	p2 *= norm.z;
	p3 *= norm.w;

	// Mix final noise value
	vec4 m = max(0.6 - vec4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.0);
	m = m * m;
	return 42.0 * dot(m * m, vec4(dot(p0, x0), dot(p1, x1), dot(p2, x2), dot(p3, x3)));
}

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

float fbm(vec3 p) {
	float value = 0.0;
	float amplitude = 0.5;
	for (int i = 0; i < 4; i++) {
		value += amplitude * snoise(p);
		p *= 2.0;
		amplitude *= 0.5;
	}
	return value;
}

void ma2in() {
	// Convert screen coordinates to a world-space direction vector
	vec4 clip = vec4(TexCoords * 2.0 - 1.0, 1.0, 1.0);
	vec4 view_ray = invProjection * clip;
	vec3 world_ray = (invView * vec4(view_ray.xy, -1.0, 0.0)).xyz;
	world_ray = normalize(world_ray);

	// Color calculations based on the y-component of the view direction
	float y = world_ray.y;
	// float y = world_ray.y + (viewPos.y * 0.005);

	// Define colors for the gradient
	vec3 twilight_color = vec3(0.9, 0.5, 0.2); // Orangey-red
	vec3 mid_sky_color = vec3(0.2, 0.4, 0.8);  // Gentle blue
	vec3 top_sky_color = vec3(0.05, 0.1, 0.3); // Dark blue

	// Blend the colors
	// float twilight_mix = smoothstep(0.0, 0.1, y+(viewPos.y/1000));
	// vec3  color = mix(twilight_color, mid_sky_color, twilight_mix);

	float t = -viewPos.y / min(world_ray.y, -0.001);
	float is_ground = step(0.0, t);
	float fog_density = 0.75;
	float fog_factor = 1.0 - exp(-t * fog_density);
	vec3  intersection_point = viewPos + world_ray * t;
	float glow_noise = fbm(0.5 * intersection_point * fbm(vec3(intersection_point.xz, 0) * 0.05 + time * 0.01));
	float final_glow = fog_factor * (1.0 + glow_noise * 0.5) * pow(max(0.0, dot(world_ray, vec3(1, 0, 0))), 4.0);
	final_glow *= 1 / (y + 0.01) +
		is_ground; // smoothstep(-0.04, -0.02, y); //is_ground; // Mask it so it stays on the "floor"
	vec3 color = mix(mid_sky_color, twilight_color, final_glow);

	// Mottled glow
	// color = mix(color, color * 0.9, snoise(world_ray * 10.0));

	float top_mix = smoothstep(0.1, 0.6, y);
	vec3  final_color = mix(color, top_sky_color, top_mix);

	// --- 2. Nebula/Haze Layer (Domain Warping + FBM) ---
	vec3  p = world_ray * 4.0;
	vec3  warp_offset = vec3(fbm(p + time * 0.05));
	float nebula_noise = fbm(p + warp_offset * 0.5);

	// Map noise to a color palette (e.g., magenta and cyan for cosmic clouds)
	vec3 nebula_palette = mix(vec3(0.0, 0.1, 0.4), vec3(0.8, 0.2, 0.7), nebula_noise);

	// Blend the nebula color into the base sky, mostly visible in the dark top_sky area
	float nebula_strength = smoothstep(0.2, 0.6, top_mix); // Only fade it in when sky is dark
	final_color = mix(final_color, nebula_palette * 1.5, nebula_strength * 0.4);

	// --- 3. Star Field Layer (Additive Blend) ---
	float stars = starLayer(world_ray);
	final_color += stars * vec3(1.0, 0.9, 0.8);

	// --- 4. "Eye of God" Light Source ---
	vec3  light_dir = normalize(lights[0].position - viewPos);
	float alignment = dot(world_ray, light_dir);

	// Create a soft, bright spot where the alignment is high
	// The smoothstep values control the size and softness of the glow.
	float eye_spot = smoothstep(0.99, 1.0, alignment);
	vec3  eye_color = vec3(1.0, 0.9, 0.7) * 0.08; // Bright, warm color

	// Create a darker "iris" for the eye effect
	// This is done by creating a thin band using two smoothsteps.
	float iris = smoothstep(0.995, 0.998, alignment) - smoothstep(0.998, 1.0, alignment);
	vec3  iris_color = vec3(0.9, 0.5, 0.2); // Orangey color from twilight

	final_color += eye_color * eye_spot * nebula_strength;
	final_color += iris_color * iris * nebula_strength;

	FragColor = vec4(final_color, 1.0);
	// FragColor = vec4(world_ray * 0.5 + 0.5, 1.0);
}

void main() {
	vec3 twilight_color = vec3(0.9, 0.5, 0.2); // Orangey-red
	vec3 mid_sky_color = vec3(0.2, 0.4, 0.8);  // Gentle blue
	vec3 top_sky_color = vec3(0.05, 0.1, 0.3); // Dark blue

	vec3 u_sunset_orange = vec3(0.9, 0.5, 0.2); // Orangey-red
	vec3 u_horizon_color = vec3(0.2, 0.4, 0.8); // Gentle blue
	vec3 u_night_color = vec3(0.05, 0.1, 0.3);  // Dark blue
	vec3 u_ground_color = vec3(0);

	// 1. Reconstruct World Ray (assuming your invView logic is working)
	// Convert screen coordinates to a world-space direction vector
	vec4 clip = vec4(TexCoords * 2.0 - 1.0, 1.0, 1.0);
	vec4 view_ray = invProjection * clip;
	vec3 world_ray = (invView * vec4(view_ray.xy, -1.0, 0.0)).xyz;
	world_ray = normalize(world_ray);

	// 2. The Sun/Light Source
	vec3  sun_dir = normalize(vec3(0.0, 0.1, -1.0)); // Near horizon
	float sun_alignment = max(0.0, dot(world_ray, sun_dir));

	// 3. Ground Intersection & Horizon Dip
	// We calculate distance to the floor to "pin" the glow.
	float ground_dist = -viewPos.y / min(world_ray.y, -0.0001);

	// 4. Calculate the "Sunset Glow"
	// Use an exponential falloff based on the world_ray.y
	// This creates a "thick" atmosphere near y=0
	float atmosphere_thickness = exp(-abs(world_ray.y) * 15.0);

	// 5. Directional Masking (Mie Scattering Approximation)
	// This concentrates the orange glow around the sun
	float glow_mask = pow(sun_alignment, 4.0) * atmosphere_thickness;

	// 6. Adding "Sunset Wisps" using your Noise
	// We sample noise based on the ray direction, but squash it horizontally
	float sunset_noise = snoise(vec3(world_ray.x * 10.0, world_ray.y * 40.0, world_ray.z * 10.0));
	float final_glow = glow_mask * (1.0 + sunset_noise * 0.4);

	// 7. Final Mixing
	vec3 sky_base = mix(u_night_color, u_horizon_color, atmosphere_thickness * 0.5);
	vec3 sunset_final = mix(sky_base, u_sunset_orange, final_glow);

	float top_mix = smoothstep(0.1, 0.6, world_ray.y);
	vec3  final_color = mix(sunset_final, top_sky_color, top_mix);

	// --- 2. Nebula/Haze Layer (Domain Warping + FBM) ---
	vec3  p = world_ray * 4.0;
	vec3  warp_offset = vec3(fbm(p + time * 0.05));
	float nebula_noise = fbm(p + warp_offset * 0.5);

	// Map noise to a color palette (e.g., magenta and cyan for cosmic clouds)
	vec3 nebula_palette = mix(vec3(0.0, 0.1, 0.4), vec3(0.8, 0.2, 0.7), nebula_noise);

	// Blend the nebula color into the base sky, mostly visible in the dark top_sky area
	float nebula_strength = mix(0.2, 0.6, top_mix); // Only fade it in when sky is dark
	final_color = mix(final_color, nebula_palette * 1.5, nebula_strength * 0.4);

	// --- 3. Star Field Layer (Additive Blend) ---
	float stars = starLayer(world_ray);
	final_color += stars * vec3(1.0, 0.9, 0.8);

	// --- 4. "Eye of God" Light Source ---
	if (num_lights > 0) {
		vec3  light_dir = normalize(lights[0].position - viewPos);
		float alignment = dot(world_ray, light_dir);

		// Create a soft, bright spot where the alignment is high
		// The smoothstep values control the size and softness of the glow.
		float eye_spot = smoothstep(0.99, 1.0, alignment);
		vec3  eye_color = lights[0].color * 0.08; // Bright, warm color

		// Create a darker "iris" for the eye effect
		// This is done by creating a thin band using two smoothsteps.
		float iris = smoothstep(0.995, 0.998, alignment) - smoothstep(0.998, 1.0, alignment);
		vec3  iris_color = lights[0].color * 0.5; // Orangey color from twilight

		final_color += eye_color * eye_spot * nebula_strength;
		final_color += iris_color * iris * nebula_strength;
	}

	FragColor = vec4(final_color, 1.0);
	Velocity = vec2(0.0);
	WorldNormal = vec3(0.0);
	MaterialData = vec4(1.0, 0.0, 1.0, 0.0);
}