#version 420 core

out vec3 FragColor;
in vec2  TexCoords;

#include "common.glsl"

vec3 calculate_transmittance(float r, float mu) {
	vec3  ro = vec3(0.0, r, 0.0);
	vec3  rd = vec3(sqrt(max(0.0, 1.0 - mu * mu)), mu, 0.0);
	float t0, t1;

	if (!intersect_sphere(ro, rd, topRadius, t0, t1))
		return vec3(1.0);

	float d = t1;
	// If it hits the planet, it's blocked
	float tp0, tp1;
	if (intersect_sphere(ro, rd, bottomRadius, tp0, tp1)) {
		if (tp0 > 0.0)
			return vec3(0.0);
	}

	const int SAMPLES = 40;
	vec3      optical_depth = vec3(0.0);
	float     ds = d / float(SAMPLES);

	for (int i = 0; i < SAMPLES; i++) {
		float            t = (float(i) + 0.5) * ds;
		vec3             p = ro + rd * t;
		float            h = length(p) - bottomRadius;
		AtmosphereSample s = sample_atmosphere(h);
		optical_depth += s.extinction * ds;
	}

	return exp(-optical_depth);
}

void main() {
	float r, mu;
	uv_to_transmittance(TexCoords, r, mu);
	FragColor = calculate_transmittance(r, mu);
}
