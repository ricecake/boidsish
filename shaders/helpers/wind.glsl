#ifndef HELPERS_WIND_GLSL
#define HELPERS_WIND_GLSL

#include "fast_noise.glsl"
#include "terrain_common.glsl"

// Wind data UBO - stores macro wind grid and simulation parameters
// The grid is 64x60 to stay comfortably within the 64KB UBO limit (4096 cells is exactly 64KB)
#ifndef WIND_DATA_BLOCK
#define WIND_DATA_BLOCK
struct WindCell {
	vec4 velocityDrag; // xyz = macro wind velocity, w = local drag factor
};

layout(std140, binding = [[WIND_DATA_BINDING]]) uniform WindData {
	ivec4 u_windOriginSize; // x, z = origin in chunks, y = size (width), w = height (60)
	vec4  u_windParams;     // x = chunkSpacing (32.0), y = time, z = curlScale, w = curlStrength
	WindCell u_windGrid[3840]; // 64 * 60 = 3840 cells. 3840 * 16 = 61440 bytes.
};
#endif

/**
 * Calculates the combined wind vector at a given world position.
 * Incorporates macro LBM-derived wind, terrain deflection, and small-scale curl noise.
 */
vec3 getWindAtPosition(vec3 worldPos) {
	if (u_windOriginSize.y <= 0) return vec3(0.0);

	float gridSpacing = u_windParams.x;
	// Measurements are at cell centers, so offset by half spacing for interpolation
	vec2 gridCoord = (worldPos.xz / gridSpacing) - vec2(u_windOriginSize.xz) - 0.5;
	ivec2 iCoord = ivec2(floor(gridCoord));
	vec2 f = fract(gridCoord);

	// 1. Bilinear interpolation of macro wind and drag
	vec4 macroData = vec4(0.0);
	int sizeX = u_windOriginSize.y;
	int sizeZ = u_windOriginSize.w;

	for (int z = 0; z <= 1; ++z) {
		for (int x = 0; x <= 1; ++x) {
			ivec2 sampleCoord = iCoord + ivec2(x, z);
			if (sampleCoord.x >= 0 && sampleCoord.x < sizeX && sampleCoord.y >= 0 && sampleCoord.y < sizeZ) {
				int idx = sampleCoord.y * sizeX + sampleCoord.x;
				float weight = (x == 0 ? 1.0 - f.x : f.x) * (z == 0 ? 1.0 - f.y : f.y);
				macroData += u_windGrid[idx].velocityDrag * weight;
			}
		}
	}

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

	// 3. Small scale chaotic noise (Curl Noise)
	// Energy pulled from macro wind by terrain drag drives local turbulence
	float time = u_windParams.y;
	float curlScale = u_windParams.z;
	float curlStrength = u_windParams.w;

	// Scale turbulence by drag and macro speed
	float turbulenceIntensity = drag * macroSpeed * curlStrength;
	vec3 curl = fastCurl3d(worldPos/50.0 * curlScale + vec3(0.0, time * 0.2, 0.0));

	// Add turbulence to macro wind
	return macroWind + curl * turbulenceIntensity;
}

#endif // HELPERS_WIND_GLSL
