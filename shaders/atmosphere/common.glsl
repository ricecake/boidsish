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
uniform float sunIntensity;
uniform float sunIntensityFactor;

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
	float H = topRadius - bottomRadius;
	float rho = sqrt(max(0.0, r * r - bottomRadius * bottomRadius));

	// Distance to top atmosphere boundary
	float d = sqrt(max(0.0, r * r * mu * mu + topRadius * topRadius - r * r)) - r * mu;
	float d_min = topRadius - r;
	float d_max = rho + sqrt(max(0.0, topRadius * topRadius - bottomRadius * bottomRadius));

	float u = clamp((d - d_min) / (d_max - d_min), 0.0, 1.0);
	float v = clamp(h / H, 0.0, 1.0);
	return vec2(u, v);
}

void uv_to_transmittance(vec2 uv, out float r, out float mu) {
	float x_mu = uv.x;
	float x_r = uv.y;

	float h = x_r * (topRadius - bottomRadius);
	r = bottomRadius + h;

	float rho = sqrt(max(0.0, r * r - bottomRadius * bottomRadius));
	float d_min = topRadius - r;
	float d_max = rho + sqrt(max(0.0, topRadius * topRadius - bottomRadius * bottomRadius));
	float d = d_min + x_mu * (d_max - d_min);

	if (d > 0.0) {
		mu = (topRadius * topRadius - r * r - d * d) / (2.0 * r * d);
	} else {
		mu = 1.0;
	}
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
	g = clamp(g, -0.999, 0.999);
	float g2 = g * g;
	return (1.0 - g2) / (4.0 * ATM_PI * pow(max(0.001, 1.0 + g2 - 2.0 * g * cos_theta), 1.5));
}

float rayleigh_phase(float cos_theta) {
	return 3.0 / (16.0 * ATM_PI) * (1.0 + cos_theta * cos_theta);
}

// Atmosphere Lookups
uniform sampler2D transmittanceLUT;
uniform sampler2D multiScatteringLUT;

vec3 get_transmittance(float r, float mu) {
	vec2 uv = transmittance_to_uv(r, mu);
	return texture(transmittanceLUT, uv).rgb;
}

vec3 get_scattering(vec3 p, vec3 rd, vec3 sun_dir, out vec3 transmittance) {
	float r = length(p);
	float mu = dot(p, rd) / max(r, 0.01);
	float mu_s = dot(p, sun_dir) / max(r, 0.01);
	float cos_theta = dot(rd, sun_dir);

	transmittance = get_transmittance(r, mu);
	vec3 trans_sun = get_transmittance(r, mu_s);

	float r_h = exp(-max(0.0, r - bottomRadius) / max(0.01, rayleighScaleHeight));
	float m_h = exp(-max(0.0, r - bottomRadius) / max(0.01, mieScaleHeight));

	vec3  rayleigh_scat = rayleighScattering * r_h * rayleigh_phase(cos_theta);
	vec3  mie_scat = vec3(mieScattering) * m_h * henyey_greenstein(cos_theta, mieAnisotropy);

	float effectiveSunIntensity = sunIntensity;
	// Assumption: num_lights and lights are defined where this is included (e.g. via lighting.glsl)
	// We use a macro or just assume they are available if included after lighting.glsl
#ifdef LIGHTING_GLSL
	if (num_lights > 0) {
		effectiveSunIntensity = lights[0].intensity * sunIntensityFactor;
	}
#endif

	vec3 scat = (rayleigh_scat + mie_scat) * trans_sun * effectiveSunIntensity;

	// Add multi-scattering
	vec2 ms_uv = vec2(mu_s * 0.5 + 0.5, (r - bottomRadius) / (topRadius - bottomRadius));
	vec3 ms = texture(multiScatteringLUT, ms_uv).rgb;
	scat += ms * effectiveSunIntensity;

	return scat;
}

#endif
