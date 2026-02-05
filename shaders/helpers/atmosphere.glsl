#ifndef HELPERS_ATMOSPHERE_GLSL
#define HELPERS_ATMOSPHERE_GLSL

#include "lighting.glsl"

// Rayleigh scattering coefficients at sea level (m^-1)
const vec3 RAYLEIGH_BETA = vec3(5.8e-6, 13.5e-6, 33.1e-6);
// Mie scattering coefficients at sea level (m^-1)
const float MIE_BETA = 21e-6;

// Scattering scale heights (m)
const float RAYLEIGH_HEIGHT = 8000.0;
const float MIE_HEIGHT = 1200.0;

// Earth radius (m)
const float EARTH_RADIUS = 6371000.0;
const float ATMOSPHERE_RADIUS = EARTH_RADIUS + 100000.0;

/**
 * Rayleigh phase function.
 */
float rayleighPhase(float cosTheta) {
	return (3.0 / (16.0 * PI)) * (1.0 + cosTheta * cosTheta);
}

/**
 * Henyey-Greenstein phase function.
 */
float henyeyGreensteinPhase(float cosTheta, float g) {
	float g2 = g * g;
	float denom = 1.0 + g2 - 2.0 * g * cosTheta;
	return (1.0 / (4.0 * PI)) * (1.0 - g2) / (denom * sqrt(denom));
}

/**
 * Samples precomputed transmittance from LUT.
 * Note: transmittanceLUT must be defined as sampler2D in the calling shader.
 * @param h Altitude in meters [0, 100000]
 * @param cosTheta Angle to zenith [-1, 1]
 */
vec3 getTransmittance(sampler2D tex, float h, float cosTheta) {
	vec2 uv = vec2(cosTheta * 0.5 + 0.5, h / 100000.0);
	return texture(tex, uv).rgb;
}

/**
 * Combined Beer-Lambert law with "Powder Effect".
 */
float beerPowder(float density, float thickness, float powderStrength) {
	float beer = exp(-density * thickness);
	float powder = 1.0 - exp(-density * thickness * 2.0);
	return mix(beer, beer * powder, powderStrength);
}

/**
 * Sky scattering approximation using Transmittance LUT.
 * Note: transmittanceLUT must be defined as sampler2D in the calling shader.
 */
vec3 calculateSkyColor(sampler2D tex, vec3 rayDir, vec3 sunDir, vec3 sunColor) {
	float cosTheta = dot(rayDir, sunDir);
	float viewZenith = rayDir.y;
	float sunZenith = sunDir.y;

	// Transmittance from observer to atmosphere boundary
	vec3 T_view = getTransmittance(tex, 0.0, viewZenith);
	// Transmittance from boundary to observer along sun direction
	vec3 T_sun = getTransmittance(tex, 0.0, sunZenith);

	// Approximate in-scattering
	float ray = rayleighPhase(cosTheta);
	float mie = henyeyGreensteinPhase(cosTheta, 0.8);

	vec3 scatteredLight = (RAYLEIGH_BETA * ray + MIE_BETA * mie) * (1.0 - T_view) * sunColor * 100000.0;

	// Add some ambient horizon color
	vec3 skyColor = mix(vec3(0.05, 0.1, 0.2), vec3(0.4, 0.6, 1.0), pow(max(0.0, viewZenith), 0.5));

	return skyColor * 0.5 + scatteredLight;
}

#endif // HELPERS_ATMOSPHERE_GLSL
