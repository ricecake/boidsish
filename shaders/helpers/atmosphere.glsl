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
 * Describes how light scatters off small particles (air molecules).
 */
float rayleighPhase(float cosTheta) {
	return (3.0 / (16.0 * PI)) * (1.0 + cosTheta * cosTheta);
}

/**
 * Henyey-Greenstein phase function.
 * Approximation for Mie scattering (larger particles like dust/water).
 * @param g Anisotropy factor [-1, 1]. g > 0 is forward scattering.
 */
float henyeyGreensteinPhase(float cosTheta, float g) {
	float g2 = g * g;
	float denom = 1.0 + g2 - 2.0 * g * cosTheta;
	return (1.0 / (4.0 * PI)) * (1.0 - g2) / (denom * sqrt(denom));
}

/**
 * Combined Beer-Lambert law with "Powder Effect".
 * Used for volumetric cloud lighting.
 */
float beerPowder(float density, float thickness, float powderStrength) {
	float beer = exp(-density * thickness);
	float powder = 1.0 - exp(-density * thickness * 2.0);
	return mix(beer, beer * powder, powderStrength);
}

/**
 * Simple analytical sky scattering approximation.
 * Returns the color of the sky in a given direction.
 */
vec3 calculateSkyColor(vec3 rayDir, vec3 sunDir, vec3 sunColor) {
	float cosTheta = dot(rayDir, sunDir);
	float zenith = max(rayDir.y, 0.0);

	// Atmosphere thickness based on angle (shorter at zenith, longer at horizon)
	float dist = 1.0 / (zenith + 0.1);

	// Rayleigh scattering
	float ray = rayleighPhase(cosTheta);
	vec3  extinction = exp(-RAYLEIGH_BETA * dist * 10000.0);
	vec3  rayleigh = RAYLEIGH_BETA * ray * (1.0 - extinction);

	// Mie scattering (sun glow)
	float mie = henyeyGreensteinPhase(cosTheta, 0.8);
	vec3  mieGlow = vec3(MIE_BETA) * mie * (1.0 - extinction);

	// Simple sky gradient
	vec3  skyColor = mix(vec3(0.1, 0.2, 0.4), vec3(0.5, 0.7, 1.0), pow(zenith, 0.5));
	vec3  scatteredLight = (rayleigh * 500000.0 + mieGlow * 100000.0) * sunColor;

	return skyColor * 0.2 + scatteredLight;
}

#endif // HELPERS_ATMOSPHERE_GLSL
