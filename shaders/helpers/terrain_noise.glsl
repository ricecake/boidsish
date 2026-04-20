#ifndef TERRAIN_NOISE_GLSL
#define TERRAIN_NOISE_GLSL

#ifndef FNC_MOD289
	#define FNC_MOD289
float mod289(float x) {
	return x - floor(x * (1.0 / 289.0)) * 289.0;
}
vec2 mod289(vec2 x) {
	return x - floor(x * (1.0 / 289.0)) * 289.0;
}
vec3 mod289(vec3 x) {
	return x - floor(x * (1.0 / 289.0)) * 289.0;
}
vec4 mod289(vec4 x) {
	return x - floor(x * (1.0 / 289.0)) * 289.0;
}
#endif

#ifndef FNC_PERMUTE
	#define FNC_PERMUTE
float permute(float x) {
	return mod289(((x * 34.0) + 1.0) * x);
}
vec2 permute(vec2 x) {
	return mod289(((x * 34.0) + 1.0) * x);
}
vec3 permute(vec3 x) {
	return mod289(((x * 34.0) + 1.0) * x);
}
vec4 permute(vec4 x) {
	return mod289(((x * 34.0) + 1.0) * x);
}
#endif

#ifndef FNC_TAYLORINVSQRT
	#define FNC_TAYLORINVSQRT
float taylorInvSqrt(float r) {
	return 1.79284291400159 - 0.85373472095314 * r;
}
vec4 taylorInvSqrt(vec4 r) {
	return 1.79284291400159 - 0.85373472095314 * r;
}
#endif

#ifndef FNC_SNOISE_VEC3
	#define FNC_SNOISE_VEC3
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
#endif

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

// Higher octave FBM for fine detail
float[6] fbm_detail(vec3 p) {
	float values[6];
	float value = 0.0;
	float amplitude = 0.5;
	for (int i = 0; i < 6; i++) {
		value += amplitude * snoise(p);
		p *= 2.0;
		amplitude *= 0.5;
		values[i] = value;
	}
	return values;
}

// Standard 2D hash for pseudo-random impulse generation
vec2 hash22(vec2 p) {
	p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
	return fract(sin(p) * 43758.5453123);
}

// pos: The world or UV coordinate being shaded
// curlVec: The sampled vector from curl texture at 'pos'
// time: Animation time to drive the wind ripples
// freq: The spatial frequency of the ripples inside the gust
// bandwidth: Controls the tightness of the Gaussian envelope (higher = smaller/sharper gusts)
// sparsity: Threshold to cull impulses (0.0 to 1.0, higher = sparser gusts)
float gaborWindNoise(vec2 pos, vec2 curlVec, float time, float freq, float bandwidth, float sparsity) {
	vec2 gridId = floor(pos);
	vec2 gridFract = fract(pos);

	float noiseAcc = 0.0;

	// Normalize the sampled curl vector to strictly control the ripple direction
	vec2 dir = length(curlVec) > 0.001 ? normalize(curlVec) : vec2(1.0, 0.0);
	vec2 F = dir * freq;

	// 3x3 grid traversal to find neighboring impulses
	for (int y = -1; y <= 1; y++) {
		for (int x = -1; x <= 1; x++) {
			vec2 neighborOffset = vec2(float(x), float(y));
			vec2 cellId = gridId + neighborOffset;

			// Generate random properties for this cell's impulse
			vec2 randVal = hash22(cellId);

			// Check for sparsity: skip this cell entirely if it doesn't meet the threshold
			if (randVal.y < sparsity)
				continue;

			// Calculate vector from fragment to the random impulse position in the neighbor cell
			vec2 impulsePos = neighborOffset + randVal;
			vec2 p = gridFract - impulsePos;

			// Distance squared for the Gaussian envelope
			float distSq = dot(p, p);

			// 1. Evaluate the Gaussian Envelope
			// e^(-pi * a^2 * d^2)
			float envelope = exp(-3.14159 * bandwidth * bandwidth * distSq);

			// 2. Evaluate the Harmonic Carrier
			// Animate phase with time, offset randomly per cell so they don't pulse synchronously
			float phase = (time * 5.0) + (randVal.x * 6.28318);
			float carrier = cos(6.28318 * dot(F, p) + phase);

			// Accumulate the result, scaling intensity by the random value for variety
			noiseAcc += randVal.y * envelope * carrier;
		}
	}

	return noiseAcc;
}

float tangentGabor(
	vec3  worldPos,
	vec3  worldNormal,
	vec3  curlVec3D,
	float time,
	float freq,
	float bandwidth,
	float sparsity
) {
	// 1. Construct the TBN frame
	vec3 N = normalize(worldNormal);

	// Choose an 'up' vector, switching to X-axis if the normal is perfectly vertical
	vec3 referenceUp = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);

	vec3 T = normalize(cross(referenceUp, N));
	vec3 B = normalize(cross(N, T));

	// 2. Project world position into 2D surface space
	vec2 surfacePos = vec2(dot(worldPos, T), dot(worldPos, B));

	// 3. Project 3D wind curl vector into 2D surface space
	vec2 surfaceCurl = vec2(dot(curlVec3D, T), dot(curlVec3D, B));

	// 4. Evaluate the 2D Gabor Noise
	return gaborWindNoise(surfacePos, surfaceCurl, time, freq, bandwidth, sparsity);
}

#endif // TERRAIN_NOISE_GLSL
