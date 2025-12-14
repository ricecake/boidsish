#version 330 core

out vec4 FragColor;

in vec3 viewDirection;

layout(std140) uniform Lighting {
	vec3  lightPos;
	vec3  viewPos;
	vec3  lightColor;
	float time;
};

uniform bool isReflectionPass;

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

vec2 fade(vec2 t) {
	return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
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

vec3 hash33(vec3 p) {
	p = fract(p * vec3(443.897, 441.423, 437.195));
	p += dot(p, p.yxz + 19.19);
	return fract((p.xxy + p.yxx) * p.zyx);
}

float starLayer(vec3 dir) {
	float scale = 100.0;
	vec3  id = floor(dir * scale);
	vec3  local_uv = fract(dir * scale);
	vec3 star_pos = hash33(id);
	float brightness = abs(sin(time / 2.0 + star_pos.x * 100.0));
	vec3  center = vec3(0.5) + (star_pos - 0.5) * 0.8;
	float dist = length(local_uv - center);
	float radius = 0.05 * brightness;
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

void main() {
	vec3 world_ray_objects = normalize(viewDirection);
	vec3 world_ray_horizon = world_ray_objects;

	if (isReflectionPass) {
		world_ray_horizon.y = -world_ray_horizon.y;
	}

	// 1. Base Sky Gradient
	float y = world_ray_horizon.y;
	vec3 twilight_color = vec3(0.9, 0.5, 0.2);
	vec3 mid_sky_color = vec3(0.2, 0.4, 0.8);
	vec3 top_sky_color = vec3(0.05, 0.1, 0.3);
	float twilight_mix = smoothstep(0.0, 0.1, y);
	vec3  color = mix(twilight_color, mid_sky_color, twilight_mix);
	color = mix(color, color * 0.9, snoise(world_ray_horizon * 10.0));
	float top_mix = smoothstep(0.1, 0.6, y);
	vec3  final_color = mix(color, top_sky_color, top_mix);

	// 2. Nebula/Haze Layer
	vec3 p = world_ray_horizon * 2.0; // Lower frequency base
	float offset_noise = snoise(p * 3.0 + time * 0.1); // A second noise to warp the coordinates
	p += offset_noise * 0.5; // Apply the warp
	vec3  warp_offset = vec3(fbm(p + time * 0.05));
	float nebula_noise = fbm(p + warp_offset * 0.2); // reduce warp strength
	vec3 nebula_palette = mix(vec3(0.0, 0.1, 0.4), vec3(0.8, 0.2, 0.7), nebula_noise);
	float nebula_strength = smoothstep(0.2, 0.6, top_mix);
	final_color = mix(final_color, nebula_palette * 1.5, nebula_strength * 0.4);

	// 3. Star Field Layer
	float stars = starLayer(world_ray_horizon);
	final_color += stars * vec3(1.0, 0.9, 0.8);

	// 4. Moon "Eye" Layer (Corrected Logic)
	vec3  light_dir = normalize(lightPos - viewPos);
	if (isReflectionPass) {
		light_dir.y = -light_dir.y;
	}
	float dot_product = dot(world_ray_objects, light_dir);

	// Define thresholds for smoothstep based on the dot product
	float moon_threshold = 0.998;  // Outer edge of the moon's white
	float iris_threshold = 0.999;
	float pupil_threshold = 0.9995;

	float moon_disc = smoothstep(moon_threshold, moon_threshold + 0.0005, dot_product);
	float iris_disc = smoothstep(iris_threshold, iris_threshold + 0.0005, dot_product);
	float pupil_disc = smoothstep(pupil_threshold, pupil_threshold + 0.0002, dot_product);

	vec3 sclera_color = vec3(0.9, 0.9, 1.0);
	vec3 iris_color = vec3(0.3, 0.8, 1.0);
	vec3 pupil_color = vec3(0.05);

	vec3 final_moon_color = mix(sclera_color, iris_color, iris_disc);
	final_moon_color = mix(final_moon_color, pupil_color, pupil_disc);

	final_color = mix(final_color, final_moon_color, moon_disc);

	FragColor = vec4(final_color, 1.0);
}
