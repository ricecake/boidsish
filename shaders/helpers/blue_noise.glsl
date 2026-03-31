#ifndef HELPERS_BLUE_NOISE_GLSL
#define HELPERS_BLUE_NOISE_GLSL

/**
 * Procedural blue noise approximation.
 * Adapted from Lygia (Patricio Gonzalez Vivo)
 * https://github.com/patriciogonzalezvivo/lygia/blob/main/color/dither/blueNoise.glsl
 *
 * This function provides a blue-noise-like distribution which avoids local clustering
 * of values, making it ideal for dithering and stochastic masking.
 */
float proceduralBlueNoise(vec2 p) {
	const float SEED1 = 1.705;
	const float size = 5.5;
	vec2        p_floor = floor(p);
	vec2        p1 = p_floor;

	// Pattern generation to avoid local correlation
	p_floor = floor(p_floor / size) * size;
	p_floor = fract(p_floor * 0.1) + 1.0 + p_floor * vec2(0.0002, 0.0003);
	float a = fract(1.0 / (0.000001 * p_floor.x * p_floor.y + 0.00001));
	a = fract(1.0 / (0.000001234 * a + 0.00001));
	float b = fract(1.0 / (0.000002 * (p_floor.x * p_floor.y + p_floor.x) + 0.00001));
	b = fract(1.0 / (0.0000235 * b + 0.00001));
	vec2 r = vec2(a, b) - 0.5;
	p1 += r * 8.12235325;

	return fract(p1.x * SEED1 + p1.y / (SEED1 + 0.15555));
}

#endif // HELPERS_BLUE_NOISE_GLSL
