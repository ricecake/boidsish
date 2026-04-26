#ifndef HELPERS_FAST_NOISE_GLSL
#define HELPERS_FAST_NOISE_GLSL

// Helper functions for fast texture-based noise lookups
// Requires noise texture samplers bound to fixed units:
// u_noiseTexture: 3D, unit 5, R=Simplex/G=Worley/B=FBM/A=Warped
// u_curlTexture: 3D, unit 6, RGB=Curl/A=FBM Curl Mag
// u_blueNoiseTexture: 2D, unit 7, RGBA tiling blue noise at 4 frequencies
// u_extraNoiseTexture: 3D, unit 8, R=Ridge/G=Gradient

#ifndef NOISE_TEXTURES_DEFINED
#define NOISE_TEXTURES_DEFINED
uniform sampler3D u_noiseTexture;
uniform sampler3D u_curlTexture;
uniform sampler2D u_blueNoiseTexture;
uniform sampler3D u_extraNoiseTexture;
uniform sampler2D u_phasorTexture;
#endif

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

// Extra Noises (from u_extraNoiseTexture)
// R: Ridge 3D
float fastRidge3d(vec3 p) {
	return texture(u_extraNoiseTexture, p).r;
}

// vec3 fastRidge3dGrad(vec3 p, float scale, out float eps) {
// 	ivec3 size = textureSize(u_extraNoiseTexture, 0);
// 	 eps = 1.0 / length(size);
// 	// vec4 gathered = textureGather(u_extraNoiseTexture, p * scale);
// 	float n = fastRidge3d(scaledFragPos);
// 	float nx = fastRidge3d((scaledFragPos + vec3(eps, 0.0, 0.0)));
// 	float nz = fastRidge3d((scaledFragPos + vec3(0.0, 0.0, eps)));

// 	return vec3(nx, n, nz);
// }


// G: Gradient 3D
float fastGradient3d(vec3 p) {
	return texture(u_extraNoiseTexture, p).g * 2.0 - 1.0;
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

/**
 * Fast 2D Phasor noise lookup.
 * Performs complex multiplication of baked phasor with runtime phase.
 * Returns the real part of the resulting complex number.
 */
float fastPhasor2d(vec2 uv, float runtimePhase) {
	vec2 baked = texture(u_phasorTexture, uv).rg;

	// Complex multiplication: (R_baked + i*I_baked) * (cos(phi) + i*sin(phi))
	// Result real part = R_baked * cos(phi) - I_baked * sin(phi)
	float cosPhi = cos(runtimePhase);
	float sinPhi = sin(runtimePhase);

	return baked.x * cosPhi - baked.y * sinPhi;
}

#endif // HELPERS_FAST_NOISE_GLSL
