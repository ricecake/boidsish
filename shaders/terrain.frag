#version 420 core
out vec4 FragColor;

in vec3 Normal;
in vec3 FragPos;
in vec2 TexCoords;

uniform sampler2D uBiomeTexture;
uniform float     uMegaTextureSize;
uniform vec2      uMegaTextureOffset;

#include "helpers/lighting.glsl"

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

// --- Insert after snoise() ---

// Calculate Curl Noise by sampling the potential field derivatives
vec3 curlNoise(vec3 p, float time) {
	const float e = 0.1; // Epsilon for finite difference

	// We create a vector potential (psi) by sampling noise at offsets.
	// We animate it by adding time to the input.
	vec3 dx = vec3(e, 0.0, 0.0);
	vec3 dy = vec3(0.0, e, 0.0);
	vec3 dz = vec3(0.0, 0.0, e);

	// Helper to sample our potential field (just 3 arbitrary noise lookups)
	// We use large offsets (100.0) to decorrelate the axes
	vec3 p_t = p + vec3(0, 0, time * 0.5);

	float x0 = snoise(p_t - dy);
	float x1 = snoise(p_t + dy);
	float x2 = snoise(p_t - dz);
	float x3 = snoise(p_t + dz);

	float y0 = snoise(p_t - dz);
	float y1 = snoise(p_t + dz);
	float y2 = snoise(p_t - dx);
	float y3 = snoise(p_t + dx);

	float z0 = snoise(p_t - dx);
	float z1 = snoise(p_t + dx);
	float z2 = snoise(p_t - dy);
	float z3 = snoise(p_t + dy);

	// Finite difference approximation of curl
	float cx = (x1 - x0) - (y1 - y0); // dPsi_z/dy - dPsi_y/dz (simplified mapping)
	float cy = (y1 - y0) - (z1 - z0);
	float cz = (z1 - z0) - (x1 - x0);

	return normalize(vec3(cx, cy, cz));
}

vec3 getFlowField(vec3 pos, vec3 normal, float time) {
	// 1. Base Wind (Global direction)
	vec3 wind = normalize(vec3(0.2, 0.0, 1.0)); // Blowing roughly Z-forward

	// 2. Downhill Vector (Gravity projected onto tangent plane)
	// This makes critters swoop into valleys.
	vec3 up = vec3(0, 1, 0);
	// Project 'down' (-up) onto the surface defined by 'normal'
	vec3 downhill = normalize((dot(normal, up) * normal) - up);

	// Fix for perfectly flat ground (downhill becomes 0)
	if (length(downhill) < 0.01)
		downhill = vec3(0);

	// 3. Curl Noise (Turbulence)
	// Scale position to control feature size of the swirls
	vec3 curl = curlNoise(pos * 0.2, time);

	// 4. Composite
	// Heavy weight on downhill to force valley following
	// Curl adds local variation
	vec3 flow = wind * 0.5 + downhill * 1.5 + curl * 0.8;

	return normalize(flow);
}

void main() {
	// discard;
	// FragColor = vec4(1,1,1, 1);

	vec2 mega_uv = (FragPos.xz - uMegaTextureOffset) / uMegaTextureSize;
	vec4 biome_sample = texture(uBiomeTexture, mega_uv);

	vec3 biome_colors[8] = vec3[](
		vec3(0.3, 0.4, 0.2), // Forest
		vec3(0.7, 0.6, 0.4), // Desert
		vec3(0.2, 0.5, 0.3), // Plains
		vec3(0.8, 0.8, 0.8), // Mountains
		vec3(0.1, 0.2, 0.4), // Ocean
		vec3(0.9, 0.2, 0.1), // Volcanic
		vec3(1.0, 1.0, 1.0), // Arctic
		vec3(0.5, 0.3, 0.6)  // Swamp
	);

	int biome1_idx = int(biome_sample.r * 255.0);
	int biome2_idx = int(biome_sample.g * 255.0);
	float blend_factor = biome_sample.b;

	vec3 color1 = biome_colors[biome1_idx];
	vec3 color2 = biome_colors[biome2_idx];
	vec3 objectColor = mix(color1, color2, blend_factor);

	vec3 norm = normalize(Normal);
	vec3 lighting = apply_lighting(FragPos, norm, objectColor, 0.2, 0.8);

	// --- Grid logic ---
	float grid_spacing = 1.0;
	vec2  coord = FragPos.xz / grid_spacing;
	vec2  f = fwidth(coord);

	vec2  grid_minor = abs(fract(coord - 0.5) - 0.5) / f;
	float line_minor = min(grid_minor.x, grid_minor.y);
	float C_minor = 1.0 - min(line_minor, 1.0);

	vec2  grid_major = abs(fract(coord / 5.0 - 0.5) - 0.5) / f;
	float line_major = min(grid_major.x, grid_major.y);
	float C_major = 1.0 - min(line_major, 1.0);

	float intensity = max(C_minor, C_major * 1.5) * length(fwidth(FragPos));
	vec3  grid_color = vec3(normalize(abs(fwidth(FragPos.zxy))) * intensity);
	vec3  result = lighting + grid_color;

	// --- Distance Fade ---
	float dist = length(FragPos.xz - viewPos.xz);
	float fade_start = 560.0;
	float fade_end = 570.0;
	float fade = 1.0 - smoothstep(fade_start, fade_end, dist + nebula_noise * 40);

	vec4 outColor = vec4(result, mix(0, fade, step(0.01, FragPos.y)));
	FragColor = mix(
		vec4(0.0, 0.7, 0.7, mix(0, fade, step(0.01, FragPos.y))) * length(outColor),
		outColor,
		step(1, fade)
	);
}
