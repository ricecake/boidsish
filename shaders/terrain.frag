#version 420 core
out vec4 FragColor;

in vec3 Normal;
in vec3 FragPos;
in vec2 TexCoords;

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

void main() {
	vec3  warp = vec3(fbm(FragPos / 50 + time * 0.05));
	float nebula_noise = fbm(FragPos / 50 + warp * 0.5);
	vec3  norm = normalize(Normal);

	// 1. Domain Warping / Noise Injection
	float noiseVal = nebula_noise;
	float distortedHeight = FragPos.y + (noiseVal * 5.0); // Variations in altitude layers

	// 2. Palette Definitions (Quasi-realistic tones)
	vec3 colWater = vec3(0.05, 0.25, 0.45);
	vec3 colGrass = vec3(0.15, 0.35, 0.1); // Forest green
	vec3 colRock = vec3(0.3, 0.28, 0.25);  // Brown-ish grey
	vec3 colSnow = vec3(0.9, 0.95, 1.0);

	// 3. Calculate Mixing Factors
	float slope = dot(norm, vec3(0, 1, 0));
	float distortedSlope = slope + (noiseVal * 0.1);

	// 4. Layer Mixing Logic

	// A. Height Biomes (Water -> Grass -> Snow)
	// Smoothstep creates soft transitions between layers
	float waterMask = 1.0 - smoothstep(1.0, 5.0, distortedHeight);
	float snowMask = smoothstep(100.0, 175.0, distortedHeight);

	vec3 biomeColor = mix(colGrass, colSnow, snowMask);
	biomeColor = mix(biomeColor, colWater, waterMask);

	// B. Slope Handling (Cliffs)
	// If the surface is steep (low dot product), we force Rock.
	// We relax the rock constraint at very high altitudes (snow sticks to walls)
	float rockThreshold = 0.7; // Adjustable: Lower = more grass, Higher = more cliffs
	float steepnessMask = smoothstep(rockThreshold, rockThreshold - 0.15, distortedSlope);

	// Don't apply rock on water or heavy snow (optional, but looks better)
	steepnessMask *= (1.0 - waterMask);
	steepnessMask *= (1.0 - smoothstep(140.0, 160.0, distortedHeight));

	vec3 finalAlbedo = mix(biomeColor, colRock, steepnessMask);

	// 5. Lighting and Output
	vec3 lighting = apply_lighting(FragPos, norm, finalAlbedo, 0.2, 0.8); // [cite: 33]

	// vec3 lighting = apply_lighting(FragPos, norm, objectColor, 0.2, 0.8);

	// --- Distance Fade ---
	float dist = length(FragPos.xz - viewPos.xz);
	float fade_start = 560.0;
	float fade_end = 570.0;
	float fade = 1.0 - smoothstep(fade_start, fade_end, dist + noiseVal * 40);

	vec4 outColor = vec4(lighting, mix(0, fade, step(0.01, FragPos.y)));
	FragColor = mix(
		vec4(0.0, 0.7, 0.7, mix(0, fade, step(0.01, FragPos.y))) * length(outColor),
		outColor,
		step(1, fade)
	);
}
