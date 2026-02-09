#ifndef TERRAIN_NOISE_GLSL
#define TERRAIN_NOISE_GLSL

#include "noise.glsl"

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

#endif // TERRAIN_NOISE_GLSL
