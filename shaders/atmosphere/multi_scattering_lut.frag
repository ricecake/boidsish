#version 420 core

out vec3 FragColor;
in vec2  TexCoords;

#include "common.glsl"

uniform sampler2D transmittanceLUT;
uniform vec3      groundAlbedo;

vec3 get_transmittance(float r, float mu) {
	vec2 uv = transmittance_to_uv(r, mu);
	return texture(transmittanceLUT, uv).rgb;
}

// Integrated radiance over the sphere
void main() {
	// TexCoords.x = sun_cos_theta, TexCoords.y = altitude
	float mu_s = TexCoords.x * 2.0 - 1.0;
	float r = bottomRadius + TexCoords.y * (topRadius - bottomRadius);

	vec3  sun_dir = vec3(sqrt(max(0.0, 1.0 - mu_s * mu_s)), mu_s, 0.0);
	const int SAMPLES = 32;

	vec3 L_scat = vec3(0.0);

	// Integrate over all directions to get multi-scattering contribution
	for (int i = 0; i < SAMPLES; i++) {
		// Low-discrepancy or random directions
		float phi = 2.0 * ATM_PI * (float(i) / float(SAMPLES));
		float theta = acos(1.0 - 2.0 * (float(i) + 0.5) / float(SAMPLES));
		vec3  rd = vec3(sin(theta) * cos(phi), cos(theta), sin(theta) * sin(phi));

		float t0, t1;
		intersect_sphere(vec3(0, r, 0), rd, topRadius, t0, t1);
		float d = t1;
		float tp0, tp1;
		if (intersect_sphere(vec3(0, r, 0), rd, bottomRadius, tp0, tp1)) {
			if (tp0 > 0.0)
				d = min(d, tp0);
		}

		// Simple one-step approximation for multi-scattering
		vec3  p = vec3(0, r, 0) + rd * (d * 0.5);
		float h = length(p) - bottomRadius;
		AtmosphereSample s = sample_atmosphere(h);
		vec3  trans = get_transmittance(r, rd.y); // Approx transmittance

		L_scat += s.rayleigh * trans; // Simplified
	}

	FragColor = L_scat / float(SAMPLES);
}
