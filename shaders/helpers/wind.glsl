#ifndef HELPERS_WIND_GLSL
#define HELPERS_WIND_GLSL

#include "fast_noise.glsl"
#include "terrain_common.glsl"

// Wind data UBO - stores macro wind grid and simulation parameters
#ifndef WIND_DATA_BLOCK
#define WIND_DATA_BLOCK
layout(std140, binding = [[WIND_DATA_BINDING]]) uniform WindData {
	ivec4 u_windOriginSize; // x, z = origin in chunks, y = size (width), w = height (60)
	vec4  u_windParams;     // x = chunkSpacing (32.0), y = time, z = curlScale, w = curlStrength
};

layout(binding = [[WIND_TEXTURE_BINDING]]) uniform sampler2D u_windTexture;
#endif

/**
 * Calculates the combined wind vector at a given world position.
 * Incorporates macro LBM-derived wind, terrain deflection, and small-scale curl noise.
 */
vec3 getWindAtPosition(vec3 worldPos) {
	if (u_windOriginSize.y <= 0) return vec3(0.0);

	float gridSpacing = u_windParams.x;
	// Measurements are at cell centers, so offset by half spacing for interpolation
	vec2 gridCoord = (worldPos.xz / gridSpacing) - vec2(u_windOriginSize.xz);

	// Normalize to [0, 1] for texture sampling
	vec2 uv = gridCoord / vec2(u_windOriginSize.y, u_windOriginSize.w);

	// 1. Hardware-accelerated bilinear interpolation of macro wind and drag
	vec4 macroData = texture(u_windTexture, uv);

	vec3 macroWind = macroData.xyz;
	float drag = macroData.w;
	float macroSpeed = length(macroWind);

	// 2. Terrain Guidance
	// Deflect wind based on terrain normal to follow slopes
	vec3 normal = getTerrainNormal(worldPos.xz);
	float terrainHeight = getTerrainHeight(worldPos.xz);

	// How close we are to the ground affects guidance strength
	float distToGround = max(0.0, worldPos.y - terrainHeight);
	float guidanceStrength = exp(-distToGround * 0.1); // Stronger near ground

	if (macroSpeed > 0.001) {
		vec3 windDir = macroWind / macroSpeed;
		// If wind is hitting the slope, push it along the surface
		float vDotN = dot(windDir, normal);
		if (vDotN < 0.0) {
			// Deflect: remove the component going into the terrain and normalize
			vec3 deflectedDir = normalize(windDir - vDotN * normal);
			macroWind = mix(macroWind, deflectedDir * macroSpeed, guidanceStrength);
		}
	}

// 3. Structured Gusts and Swirls
	// Energy pulled from macro wind by terrain drag drives local turbulence.
	// We introduce "gustiness" by modulating turbulence scale and intensity with large-scale noise.
	float time = u_windParams.y;
	float curlScale = u_windParams.z;
	float curlStrength = u_windParams.w;

	// Gustiness: large-scale noise that moves with the macro wind
	// This creates areas of high/low turbulence that feel like structured gusts.
	float gustAdvectionSpeed = 0.5;
	vec3 gustPos = worldPos - (macroWind * time * gustAdvectionSpeed);
	float gustiness = fastSimplex3d(gustPos * 0.005) * 0.5 + 0.5;

	// Scale turbulence intensity by drag, macro speed, and the structured gust factor
	float turbulenceIntensity = drag * macroSpeed * curlStrength * (0.2 + 0.8 * gustiness);

	// Advect the sampling coordinates downstream using the macro wind.
	float advectionSpeed = 1.0;
	vec3 advectedPos = worldPos - (macroWind * time * advectionSpeed);

	// Sample the curl noise using the moving coordinate space
	// We modulate the curl scale slightly by gustiness to add variety to swirl sizes
	float dynamicCurlScale = curlScale * (0.8 + 0.4 * gustiness);
	vec3 curl = fastCurl3d(advectedPos/12.0 * dynamicCurlScale + vec3(0.0, time * 0.02, 0.0));

	// 4. Combined Result
	// Instead of just adding curl, we use it to perturb the direction of the macro wind,
	// creating the effect of chaotic swirls within the flow.
	if (macroSpeed > 0.001) {
		// Use curl to rotate the macro wind vector slightly
		vec3 rotationAxis = normalize(curl + vec3(0.0, 1.0, 0.0)); // Bias axis toward Up
		float rotationAngle = turbulenceIntensity * 0.15;

		float cosTheta = cos(rotationAngle);
		float sinTheta = sin(rotationAngle);

		macroWind = macroWind * cosTheta +
					cross(rotationAxis, macroWind) * sinTheta +
					rotationAxis * dot(rotationAxis, macroWind) * (1.0 - cosTheta);
	}

	// Add a final additive turbulence component for high-frequency jitter
	return macroWind + curl * (turbulenceIntensity * 0.3);
}

#endif // HELPERS_WIND_GLSL
