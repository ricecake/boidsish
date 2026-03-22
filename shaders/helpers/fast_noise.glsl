#ifndef FAST_NOISE_GLSL
#define FAST_NOISE_GLSL

// Helper functions for fast texture-based noise lookups
// Requires noise texture samplers bound to fixed units:
// u_noiseTexture: 3D, unit 5, R=Simplex/G=Worley/B=FBM/A=Warped
// u_curlTexture: 3D, unit 6, RGB=Curl/A=FBM Curl Mag
// u_blueNoiseTexture: 2D, unit 7, RGBA tiling blue noise at 4 frequencies

uniform sampler3D u_noiseTexture;
uniform sampler3D u_curlTexture;
uniform sampler2D u_blueNoiseTexture;

// R: Simplex 3D
float fastSimplex3d(vec3 p) {
	return texture(u_noiseTexture, p).r * 2.0 - 1.0;
}

// G: Worley 3D
float fastWorley3d(vec3 p) {
	return texture(u_noiseTexture, p).g;
}

// B: FBM 3D
float fastFbm3d(vec3 p) {
	return texture(u_noiseTexture, p).b * 2.0 - 1.0;
}

// A: Warped FBM 3D
float fastWarpedFbm3d(vec3 p) {
	return texture(u_noiseTexture, p).a * 2.0 - 1.0;
}

// Multi-octave texture FBM
float fastTextureFbm(vec3 p, int octaves) {
	float value = 0.0;
	float amplitude = 0.5;
	for (int i = 0; i < octaves; i++) {
		value += amplitude * (texture(u_noiseTexture, p).r * 2.0 - 1.0);
		p *= 2.0;
		amplitude *= 0.5;
	}
	return value;
}

// Curl Noise lookup
vec3 fastCurl3d(vec3 p) {
	return texture(u_curlTexture, p).rgb;
}

// FBM Curl magnitude lookup
float fastFbmCurl3d(vec3 p) {
	return texture(u_curlTexture, p).a;
}

// Blue Noise lookups (at different frequencies)
float fastBlueNoise(vec2 uv, int frequencyIndex) {
	vec4 bn = texture(u_blueNoiseTexture, uv);
	if (frequencyIndex == 0)
		return bn.r;
	if (frequencyIndex == 1)
		return bn.g;
	if (frequencyIndex == 2)
		return bn.b;
	return bn.a;
}

float fastBlueNoise(vec2 uv) {
	return texture(u_blueNoiseTexture, uv).r;
}

#endif
