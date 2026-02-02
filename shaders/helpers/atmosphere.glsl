#ifndef HELPERS_ATMOSPHERE_GLSL
#define HELPERS_ATMOSPHERE_GLSL

// This helper pulls in the Lighting UBO and shadow functions
#include "lighting.glsl"

const float Re = 6371e3; // Earth radius
const float Ra = 6471e3; // Atmosphere radius
const float Hr = 8e3;    // Rayleigh scale height
const float Hm = 1.2e3;  // Mie scale height

const vec3  betaR = vec3(5.8e-6, 13.5e-6, 33.1e-6); // Rayleigh scattering coefficients
const float betaM = 21e-6;                         // Mie scattering coefficient

float rayleighPhase(float cosTheta) {
	// PI is defined in helpers/lighting.glsl
	return 3.0 / (16.0 * PI) * (1.0 + cosTheta * cosTheta);
}

float miePhase(float cosTheta, float g) {
	float g2 = g * g;
	return 3.0 / (8.0 * PI) * ((1.0 - g2) * (1.0 + cosTheta * cosTheta)) /
		((2.0 + g2) * pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5));
}

vec2 getDensity(float height) {
	return exp(-max(0.0, height) / vec2(Hr, Hm));
}

bool intersectSphere(vec3 ro, vec3 rd, float r, out float t0, out float t1) {
	float b = dot(rd, ro);
	float c = dot(ro, ro) - r * r;
	float h = b * b - c;
	if (h < 0.0)
		return false;
	h = sqrt(h);
	t0 = -b - h;
	t1 = -b + h;
	return true;
}

vec2 opticalDepth(vec3 ro, vec3 rd, float L, int samples) {
	float stepSize = L / float(samples);
	vec2  od = vec2(0.0);
	for (int i = 0; i < samples; i++) {
		vec3  p = ro + rd * (float(i) + 0.5) * stepSize;
		float h = length(p) - Re;
		od += getDensity(h) * stepSize;
	}
	return od;
}

vec3 calculateScattering(vec3 ro, vec3 rd, float L, int lightIndex, int samples) {
	vec3  lightDir = normalize(-lights[lightIndex].direction);
	vec3  lightColor = lights[lightIndex].color * lights[lightIndex].intensity;
	float stepSize = L / float(samples);
	vec2  odView = vec2(0.0);
	vec3  totalR = vec3(0.0);
	vec3  totalM = vec3(0.0);

	float cosTheta = dot(rd, lightDir);
	float pR = rayleighPhase(cosTheta);
	float pM = miePhase(cosTheta, 0.76);

	for (int i = 0; i < samples; i++) {
		vec3  p = ro + rd * (float(i) + 0.5) * stepSize;
		float h = length(p) - Re;
		vec2  d = getDensity(h) * stepSize;
		odView += d;

		float t0, t1;
		float tp0, tp1;
		bool  planetBlocked = intersectSphere(p, lightDir, Re, tp0, tp1) && tp0 > 0.0;

		if (!planetBlocked && intersectSphere(p, lightDir, Ra, t0, t1) && t1 > 0.0) {
			// Engine coordinate offset for shadow sampling
			vec3 engineP = p - vec3(0.0, Re, 0.0);

			// Check terrain shadows
			float shadow = calculateShadow(lightIndex, engineP, lightDir, lightDir);

			vec2 odLight = opticalDepth(p, lightDir, t1, 4);
			vec3 tau = betaR * (odView.x + odLight.x) + betaM * 1.1 * (odView.y + odLight.y);
			vec3 attenuation = exp(-tau);
			totalR += d.x * attenuation * shadow;
			totalM += d.y * attenuation * shadow;
		}
	}

	return lightColor * (totalR * betaR * pR + totalM * betaM * pM);
}

#endif // HELPERS_ATMOSPHERE_GLSL
