#ifndef ATMOSPHERE_COMMON_GLSL
#define ATMOSPHERE_COMMON_GLSL

// Atmosphere Parameters (should match C++ struct)
uniform vec3  rayleighScattering;
uniform float rayleighScaleHeight;
uniform float mieScattering;
uniform float mieExtinction;
uniform float mieScaleHeight;
uniform vec3  absorptionExtinction;
uniform float bottomRadius;
uniform float topRadius;
uniform float mieAnisotropy;

const float ATM_PI = 3.14159265359;

struct AtmosphereSample {
	vec3  rayleigh;
	vec3  mie;
	vec3  extinction;
};

AtmosphereSample sample_atmosphere(float h) {
	float r_h = exp(-h / rayleighScaleHeight);
	float m_h = exp(-h / mieScaleHeight);

	AtmosphereSample s;
	s.rayleigh = rayleighScattering * r_h;
	s.mie = vec3(mieScattering * m_h);
	s.extinction = s.rayleigh + vec3(mieExtinction * m_h) + absorptionExtinction; // Simplified absorption
	return s;
}

// Mapping functions for LUTs
vec2 transmittance_to_uv(float r, float mu) {
	float h = r - bottomRadius;
	float rho = sqrt(max(0.0, r * r - bottomRadius * bottomRadius));
	float d = sqrt(max(0.0, r * r - bottomRadius * bottomRadius * (1.0 - mu * mu))) - rho;
	float d_max = sqrt(max(0.0, topRadius * topRadius - bottomRadius * bottomRadius));

	float u = d / d_max;
	float v = h / (topRadius - bottomRadius);
	return vec2(u, v);
}

void uv_to_transmittance(vec2 uv, out float r, out float mu) {
	float h = uv.y * (topRadius - bottomRadius);
	r = bottomRadius + h;

	float d_max = sqrt(max(0.0, topRadius * topRadius - bottomRadius * bottomRadius));
	float rho = sqrt(max(0.0, r * r - bottomRadius * bottomRadius));
	float d = uv.x * d_max;

	mu = (d * d + 2.0 * d * rho) / (2.0 * r);
	mu = clamp(mu, -1.0, 1.0);
}

// Ray-Sphere intersection
bool intersect_sphere(vec3 ro, vec3 rd, float radius, out float t0, out float t1) {
	float b = dot(ro, rd);
	float c = dot(ro, ro) - radius * radius;
	float det = b * b - c;
	if (det < 0.0)
		return false;
	det = sqrt(det);
	t0 = -b - det;
	t1 = -b + det;
	return true;
}

float henyey_greenstein(float cos_theta, float g) {
	float g2 = g * g;
	return (1.0 - g2) / (4.0 * ATM_PI * pow(1.0 + g2 - 2.0 * g * cos_theta, 1.5));
}

float rayleigh_phase(float cos_theta) {
	return 3.0 / (16.0 * ATM_PI) * (1.0 + cos_theta * cos_theta);
}

#endif
