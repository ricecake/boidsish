// Helper functions for fast texture-based noise lookups
// Requires a 3D texture sampler named 'u_noiseTexture' bound to some unit.

#ifndef HELPERS_FAST_NOISE_GLSL
#define HELPERS_FAST_NOISE_GLSL

#extension GL_ARB_bindless_texture : enable

#ifndef BINDLESS_SUPPORTED
uniform sampler3D u_noiseTexture;
uniform sampler3D u_curlTexture;
#endif

#ifdef BINDLESS_SUPPORTED
// These will be provided by the including shader if it supports MDI/Bindless
vec4 sampleNoise(vec3 p);
vec4 sampleCurl(vec3 p);
#else
// Fallback to standard uniforms
vec4 sampleNoise(vec3 p) {
	return texture(u_noiseTexture, p);
}
vec4 sampleCurl(vec3 p) {
	return texture(u_curlTexture, p);
}
#endif

// R: Simplex 3D
float fastSimplex3d(vec3 p) {
	return sampleNoise(p).r * 2.0 - 1.0;
}

// G: Worley 3D
float fastWorley3d(vec3 p) {
	return sampleNoise(p).g;
}

// B: FBM 3D
float fastFbm3d(vec3 p) {
	return sampleNoise(p).b * 2.0 - 1.0;
}

// A: Warped FBM 3D
float fastWarpedFbm3d(vec3 p) {
	return sampleNoise(p).a * 2.0 - 1.0;
}

// Multi-octave texture FBM
float fastTextureFbm(vec3 p, int octaves) {
	float value = 0.0;
	float amplitude = 0.5;
	for (int i = 0; i < octaves; i++) {
		value += amplitude * (sampleNoise(p).r * 2.0 - 1.0);
		p *= 2.0;
		amplitude *= 0.5;
	}
	return value;
}

// Curl Noise lookup
vec3 fastCurl3d(vec3 p) {
	return sampleCurl(p).rgb;
}

// FBM Curl magnitude lookup
float fastFbmCurl3d(vec3 p) {
	return sampleCurl(p).a;
}

#endif
