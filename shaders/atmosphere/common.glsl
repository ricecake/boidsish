
#include "../helpers/constants.glsl"

// Physical Constants
const float kEarthRadius = 6360.0; // km

#ifndef ATMOSPHERE_HEIGHT_DEFINED
	#define ATMOSPHERE_HEIGHT_DEFINED
uniform float u_atmosphereHeight;
#endif

#ifndef TRANSMITTANCE_LUT_DEFINED
	#define TRANSMITTANCE_LUT_DEFINED
uniform sampler2D u_transmittanceLUT;
#endif

#define kAtmosphereHeight u_atmosphereHeight
#define kTopRadius (kEarthRadius + kAtmosphereHeight)

uniform vec3  u_rayleighScatteringBase;
uniform float u_rayleighScaleHeight;
#define kRayleighScattering u_rayleighScatteringBase
#define kRayleighScaleHeight u_rayleighScaleHeight

uniform float u_mieScatteringBase;
uniform float u_mieExtinctionBase;
uniform float u_mieScaleHeight;
#define kMieScattering u_mieScatteringBase
#define kMieExtinction u_mieExtinctionBase
#define kMieScaleHeight u_mieScaleHeight

uniform float u_rayleighScale;
uniform float u_mieScale;
uniform float u_mieAnisotropy;

uniform vec3 u_ozoneAbsorptionBase;
#define kOzoneAbsorption u_ozoneAbsorptionBase

// Helper functions
bool intersectSphere(vec3 ro, vec3 rd, float radius, out float t0, out float t1) {
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

float getRayleighDensity(float h) {
	return exp(-max(0.0, h) / kRayleighScaleHeight);
}

float getMieDensity(float h) {
	return exp(-max(0.0, h) / kMieScaleHeight);
}

float getOzoneDensity(float h) {
	return max(0.0, 1.0 - abs(max(0.0, h) - 25.0) / 15.0);
}

struct Sampling {
	vec3 rayleigh;
	vec3 mie;
	vec3 extinction;
};

Sampling getAtmosphereProperties(float h) {
	float rd = getRayleighDensity(h);
	float md = getMieDensity(h);
	float od = getOzoneDensity(h);

	Sampling s;
	s.rayleigh = kRayleighScattering * rd * u_rayleighScale;
	s.mie = vec3(kMieScattering * md * u_mieScale);
	s.extinction = s.rayleigh + vec3(kMieExtinction * md * u_mieScale) + kOzoneAbsorption * od;
	return s;
}

// Phase Functions
float rayleighPhase(float cosTheta) {
	return 3.0 / (16.0 * PI) * (1.0 + cosTheta * cosTheta);
}

float miePhase(float cosTheta) {
	float g = u_mieAnisotropy;
	float g2 = g * g;
	return (1.0 - g2) / (4.0 * PI * pow(max(1e-4, 1.0 + g2 - 2.0 * g * cosTheta), 1.5));
}

// LUT mapping functions - Simple Linear mapping for Transmittance to avoid precision issues
vec2 transmittanceToUV(float r, float mu) {
	float x_mu = mu * 0.5 + 0.5;
	float x_r = (r - kEarthRadius) / kAtmosphereHeight;
	return vec2(x_mu, x_r);
}

void UVToTransmittance(vec2 uv, out float r, out float mu) {
	mu = uv.x * 2.0 - 1.0;
	r = kEarthRadius + uv.y * kAtmosphereHeight;
}

