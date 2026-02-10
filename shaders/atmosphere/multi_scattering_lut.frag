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
	const int SAMPLES = 64;

	vec3 L_acc = vec3(0.0);
	vec3 f_ms_acc = vec3(0.0);

	// Integrate over all directions to get multi-scattering contribution
	for (int i = 0; i < SAMPLES; i++) {
		float phi = 2.0 * ATM_PI * (float(i) / float(SAMPLES));
		float theta = acos(1.0 - 2.0 * (float(i) + 0.5) / float(SAMPLES));
		vec3  rd = vec3(sin(theta) * cos(phi), cos(theta), sin(theta) * sin(phi));

		float t0, t1;
		intersect_sphere(vec3(0, r, 0), rd, topRadius, t0, t1);
		float d = t1;
		bool hit_ground = false;
		float tp0, tp1;
		if (intersect_sphere(vec3(0, r, 0), rd, bottomRadius, tp0, tp1)) {
			if (tp0 > 0.0) {
				d = min(d, tp0);
				hit_ground = true;
			}
		}

		// Single Scattering approximation for this direction
		const int STEP_SAMPLES = 8;
		float ds = d / float(STEP_SAMPLES);
		vec3 L_dir = vec3(0.0);
		vec3 trans_acc = vec3(1.0);

		for(int j = 0; j < STEP_SAMPLES; j++) {
			float t = (float(j) + 0.5) * ds;
			vec3 p = vec3(0, r, 0) + rd * t;
			float h = length(p) - bottomRadius;
			AtmosphereSample s = sample_atmosphere(h);

			float mu_s_p = dot(normalize(p), sun_dir);
			vec3 sun_trans = get_transmittance(length(p), mu_s_p);

			// We use an isotropic phase function (1/4pi) for the multi-scattering source
			vec3 scat = (s.rayleigh + s.mie) * sun_trans;

			L_dir += trans_acc * scat * ds;
			trans_acc *= exp(-s.extinction * ds);
		}

		if (hit_ground) {
			vec3 sun_trans = get_transmittance(bottomRadius, mu_s);
			L_dir += trans_acc * groundAlbedo * sun_trans * max(0.0, mu_s) / ATM_PI;
		}

		L_acc += L_dir;

		// Fraction of light that is scattered (to approximate multiple orders)
		AtmosphereSample s_at_r = sample_atmosphere(r - bottomRadius);
		f_ms_acc += (s_at_r.rayleigh + s_at_r.mie) / max(vec3(0.0001), s_at_r.extinction);
	}

	vec3 L_avg = L_acc / float(SAMPLES);
	vec3 f_ms = f_ms_acc / float(SAMPLES);

	// Higher order scattering orders approximated as a geometric series
	vec3 L_ms = L_avg / (vec3(1.0) - f_ms * 0.9); // 0.9 factor for conservative energy

	FragColor = L_ms;
}
