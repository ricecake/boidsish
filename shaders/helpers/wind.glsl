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
	float time = u_windParams.y;
	float curlScale = u_windParams.z;
	float curlStrength = u_windParams.w;
	float wrappedTime = mod(time, 128.0);

	// Extract normalized direction for predictable math
	vec2 windDir2D = macroSpeed > 0.001 ? macroWind.xz / macroSpeed : vec2(1.0, 0.0);

	// Gustiness: Swap Worley for a smoother noise to get rolling packets, not sharp cells
	float gustAdvectionSpeed = 0.05;
	vec3 gustPos = worldPos - (macroWind * wrappedTime * gustAdvectionSpeed);

	// Assuming fastSimplex3d exists in your noise library. If not, smooth the Worley aggressively.
	// float gustiness = smoothstep(0.2, 0.8, fastSimplex3d(gustPos / 250.0) * 0.5 + 0.5);
	float gustiness = fastSimplex3d(gustPos / 250.0) * 0.5 + 0.5;

	// 4. Phasor Ripples (The "Packets")
	// rippleFreq controls the physical size of the gust packets (how often the Gabor kernels appear)
	float rippleFreq = 0.0001;
	vec2 rippleUV = worldPos.xz * rippleFreq;

	// rippleTightness controls the spatial frequency of the waves INSIDE the packet
	float rippleTightness = 0.05;
	float ripplePhaseSpeed = 5.0; // Needs to be higher since we normalized the vector

	// Normalize the dot product to decouple wave tightness from wind speed
	float phaseShift = dot(windDir2D, worldPos.xz) * rippleTightness - (wrappedTime * ripplePhaseSpeed);
	float rawPhasor = fastPhasor2d(rippleUV, phaseShift);

	// Remap to strictly positive to prevent "inverse gusts" (wind dying down)
	float positiveRipple = rawPhasor * 0.5 + 0.5;

	// 5. Apply the Gust Surge
	// Instead of perpendicular wiggles, add a forward surge along the wind direction
	if (macroSpeed > 0.001) {
		float surgeStrength = 1.5; // How much harder the gust blows
		// Multiply by the gustiness envelope so the ripples only appear inside the macro gusts
		float localizedSurge = positiveRipple * gustiness * surgeStrength * macroSpeed;

		macroWind.xz += windDir2D * localizedSurge;
	}

	// 6. Local Turbulence (Curl)
	float advectionSpeed = 0.250;
	vec3 advectedPos = worldPos - (macroWind * wrappedTime * advectionSpeed);

	// dynamicCurlScale adds variety to swirl sizes
	float dynamicCurlScale = curlScale * (0.8 + 0.4 * gustiness);
	vec3 curl = fastCurl3d(advectedPos/200.0 * dynamicCurlScale + vec3(0.0, wrappedTime * 0.02, 0.0));

	float turbulenceIntensity = drag * macroSpeed * curlStrength;

	// Apply curl cleanly as a final perturbation, removing the complex rotation logic
	// return (macroWind + curl * turbulenceIntensity) * smoothstep(0.75, 0.80, positiveRipple);//pow(gustiness, 3);
	// return (macroWind) * smoothstep(0.0, 0.5, step(0.85, positiveRipple));//pow(gustiness, 3);
	// return ((macroWind + curl * turbulenceIntensity)) * smoothstep(0.45, 1.0, positiveRipple);//pow(gustiness, 3);
	return ((macroWind + curl * turbulenceIntensity)) * smoothstep(0.45 * smoothstep(50.0, 0.0, length(macroWind)), 1.0, positiveRipple);//pow(gustiness, 3);
}

#endif // HELPERS_WIND_GLSL
