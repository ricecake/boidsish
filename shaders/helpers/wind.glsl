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
	// return macroWind;
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
	// Use continuous time. Do not wrap it, or the phase math will fracture.
	float time = u_windParams.y;
	float curlScale = u_windParams.z;
	float curlStrength = u_windParams.w;

	// Extract normalized direction for predictable math
	vec2 windDir2D = macroSpeed > 0.001 ? macroWind.xz / macroSpeed : vec2(1.0, 0.0);

	// Gustiness: Smoothstep the simplex to create wide, rolling "valleys" of stillness
	float gustAdvectionSpeed = 0.75;
	vec3 gustPos = worldPos - (macroWind * time * gustAdvectionSpeed);
	float gustiness = smoothstep(0.1, 0.7, fastSimplex3d(gustPos / 250.0) * 0.5 + 0.5);

	// 4. Phasor Ripples (The "Packets")
	// Frequency and phase speed adapt to macro speed to prevent high-frequency chaos.
	// As speed increases, we widen the ripples and slow down their oscillation.
	float speedSmoothing = 1.0 / (1.0 + macroSpeed * 0.05);

	float rippleFreq = 0.005 * speedSmoothing;
	vec2 rippleUV = worldPos.xz * rippleFreq;

	float rippleTightness = 0.05;
	float ripplePhaseSpeed = 5.0 * speedSmoothing;

	float phaseShift = dot(windDir2D, worldPos.xz) * rippleTightness - (time * ripplePhaseSpeed);
	float rawPhasor = fastPhasor2d(rippleUV, phaseShift);

	// Remap and apply an asymmetric power curve to fix the "pulling" vertex snap-back
	// This makes the gust hit quickly, but release slowly.
	float positiveRipple = pow(rawPhasor * 0.5 + 0.5, 2.0);

	// 5. The Stillness Filter
	// Attenuate the base wind during lulls, but ONLY if the macro wind is relatively weak.
	// Storms (high macroSpeed) will ignore the lull and blow continuously.
	float calmThreshold = smoothstep(50.0, 0.0, macroSpeed);
	float baseWindMultiplier = mix(1.0, gustiness, calmThreshold);

	vec3 finalWind = macroWind * baseWindMultiplier;

	// 6. Apply the Gust Surge
	if (macroSpeed > 0.001) {
		float surgeStrength = 2.5;
		// The surge only exists inside the macro gusts
		float localizedSurge = positiveRipple * gustiness * surgeStrength * macroSpeed;
		finalWind.xz += windDir2D * localizedSurge;
	}

	// 7. Local Turbulence (Curl)
	// Scale and temporal drift also adapt to macro speed for smoother transitions at high intensity.
	float dynamicCurlScale = curlScale * (0.8 + 0.4 * gustiness) * speedSmoothing;
	vec3 advectedPos = worldPos - (finalWind * time * 0.250);

	// Intensity is dampened slightly at very high speeds to maintain structure
	vec3 curl = fastCurl3d(advectedPos/200.0 * dynamicCurlScale + vec3(0.0, time * 0.02 * speedSmoothing, 0.0));
	float turbulenceIntensity = drag * length(finalWind) * curlStrength * speedSmoothing;

	// Modulate turbulence intensity and introduce a subtle directional shift to the flow
	turbulenceIntensity *= (0.8 + 0.4 * (positiveRipple * 0.5 + 0.5));
	if (macroSpeed > 0.001) {
		// Apply a small perpendicular shift based on the ripple
		vec2 perpWind = vec2(-macroWind.z, macroWind.x) / macroSpeed;
		macroWind.xz += perpWind * (positiveRipple * macroSpeed * 0.15);
	}

	// 5. Combined Result
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

	// Add curl as a final perturbation
	return finalWind + curl * turbulenceIntensity;
}

#endif // HELPERS_WIND_GLSL
