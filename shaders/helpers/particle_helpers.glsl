
// Basic pseudo-random number generator
float rand(vec2 co) {
	return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

// 3D random number generator
vec3 rand3(vec2 co) {
	return vec3(rand(co + vec2(0.1, 0.2)), rand(co + vec2(0.3, 0.4)), rand(co + vec2(0.5, 0.6)));
}

// A standard 32-bit integer hash
uint hash(uint x) {
	x ^= x >> 16;
	x *= 0x7feb352dU;
	x ^= x >> 15;
	x *= 0x846ca68bU;
	x ^= x >> 16;
	return x;
}

// Convert the hashed integer to a float in [0.0, 1.0]
float randomFloat(uint state) {
	return float(hash(state)) / 4294967295.0; // Divide by 0xFFFFFFFF
}

// Calculate Curl Noise by sampling the pre-calculated curl texture
vec3 curlNoise(vec3 p, float time, sampler3D curlTexture) {
	float noiseScale = 0.02;
	vec3  lookupPos = p * noiseScale + vec3(0, 0, time * 0.1);
	return texture(curlTexture, lookupPos).rgb;
}

float fbmCurlMagnitude(vec3 p, float time, sampler3D curlTexture) {
	float noiseScale = 0.02;
	vec3  lookupPos = p * noiseScale + vec3(0, 0, time * 0.1);
	return texture(curlTexture, lookupPos).a;
}

