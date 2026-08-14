#ifndef HELPERS_WIND_GLSL
#define HELPERS_WIND_GLSL

#include "fast_noise.glsl"
#include "terrain_common.glsl"
#include "lygia/generative/psrdnoise.glsl"
#include "lygia/generative/gerstnerWave.glsl"
#ifdef PI
#undef PI
#endif

// Wind data UBO - stores macro wind grid and simulation parameters
#ifndef WIND_DATA_BLOCK
#define WIND_DATA_BLOCK
layout(std140, binding = [[WIND_DATA_BINDING]]) uniform WindData {
	ivec4 u_windOriginSize; // x, z = origin in chunks, y = size (width), w = height (60)
	vec4  u_windParams;     // x = chunkSpacing (32.0), y = time, z = curlScale, w = curlStrength
};
#endif

#ifndef WIND_TEXTURES_DEFINED
#define WIND_TEXTURES_DEFINED
layout(binding = [[WIND_TEXTURE_BINDING]]) uniform sampler2D u_windTexture;
#ifdef WIND_COMPUTE
layout(binding = [[LBM_WIND_TEXTURE_BINDING]]) uniform sampler2D u_lbmWindTexture;
#endif

layout(binding = [[WEATHER_SCALARS_BINDING]]) uniform sampler2D u_weatherScalarsTexture;
layout(binding = [[WEATHER_AEROSOLS_BINDING]]) uniform sampler2D u_weatherAerosolsTexture;
#endif

/**
 * Simple Phacelle Noise (2D)
 * Approximates highly directional phasor noise using a 16-sample kernel.
 *
 * @param uv  - The sampling position. Scale this to change the density of the noise cells.
 * @param dir - The desired direction of the ripples. Does not need to be normalized if
 *              you want direction magnitude to influence the phase gradient.
 * @return    - A normalized vec2 representing [cos(phase), sin(phase)].
 *              Extract the .x component for the base wave pattern.
 */
vec2 fastSimplePhacelle2d(vec2 uv, vec2 dir) {
    vec2 cell = floor(uv);
    vec2 frac = fract(uv);

    float sumCos = 0.0;
    float sumSin = 0.0;
    float sumWeight = 0.0;

    // Evaluate 4x4 grid for smooth overlapping kernels
    for (int y = -1; y <= 2; y++) {
        for (int x = -1; x <= 2; x++) {
            vec2 offset = vec2(float(x), float(y));
            vec2 neighborCell = cell + offset;

            // Generate a random, static phase shift for this specific cell [0, 2PI]
            // Note: Replace hash21 with whatever 2D->1D hash exists in your fast_noise.glsl
            float cellPhase = fract(sin(dot(neighborCell, vec2(12.9898, 78.233))) * 43758.5453) * 6.28318530718;

            // Vector from current sampling point to the neighboring cell's origin
            vec2 delta = offset - frac;
            float distSq = dot(delta, delta);

            // Kernel falloff weight (using a fast polynomial approximation of a Gaussian)
            // Cutoff at squared distance 4.0
            float weight = max(0.0, 1.0 - distSq * 0.25);
            weight = weight * weight * weight;

            // Project the spatial delta onto the desired direction vector to align the wave,
            // then add the cell's random phase offset.
            float phase = cellPhase + dot(dir, delta) * 3.14159265;

            // Accumulate both wave components
            sumCos += cos(phase) * weight;
            sumSin += sin(phase) * weight;
            sumWeight += weight;
        }
    }

    // Normalize the accumulated vector to rebuild a clean phase
    return normalize(vec2(sumCos, sumSin) / (sumWeight + 0.0001));
}

/**
 * Fast lookup for pre-integrated wind data.
 */
vec3 getWindAtPosition(vec3 worldPos) {
	if (u_windOriginSize.y <= 0) return vec3(0.0);

	float gridSpacing = u_windParams.x;
	vec2 gridCoord = (worldPos.xz / gridSpacing) - vec2(u_windOriginSize.xz);
	vec2 uv = gridCoord / vec2(u_windOriginSize.y, u_windOriginSize.w);

	vec3 wind = texture(u_windTexture, uv).xyz;

	vec2 phacelleOut = fastSimplePhacelle2d(uv * 1024.0, normalize(wind.xz));
	float phaseShift = -u_windParams.y * 0.50;
	float cosShift = cos(phaseShift);
	float sinShift = sin(phaseShift);

	float rawPhacelle = phacelleOut.x * cosShift - phacelleOut.y * sinShift;
	float positiveRipple = rawPhacelle * 0.5 + 0.5;

	// return wind * (positiveRipple);
	return wind * (1.0-(smoothstep(0,0.25,positiveRipple)*(1.0-smoothstep(0.25,1.0, positiveRipple))));
	// return wind * (0.5+positiveRipple);
	// return gerstnerWave(uv, normalize(wind.xz), 0.5, 64.0, u_windParams.y);

}

/**
 * Get weather scalars (x: temperature, y: humidity, z: pressure, w: viscosityDamping)
 */
vec4 getWeatherScalarsAtPosition(vec3 worldPos) {
	if (u_windOriginSize.y <= 0) return vec4(0.0);

	float gridSpacing = u_windParams.x;
	vec2 gridCoord = (worldPos.xz / gridSpacing) - vec2(u_windOriginSize.xz);
	vec2 uv = gridCoord / vec2(u_windOriginSize.y, u_windOriginSize.w);

	return texture(u_weatherScalarsTexture, uv);
}

/**
 * Get weather aerosols (x, y, z, w: concentrations of 4 aerosol types)
 */
vec4 getWeatherAerosolsAtPosition(vec3 worldPos) {
	if (u_windOriginSize.y <= 0) return vec4(0.0);

	float gridSpacing = u_windParams.x;
	vec2 gridCoord = (worldPos.xz / gridSpacing) - vec2(u_windOriginSize.xz);
	vec2 uv = gridCoord / vec2(u_windOriginSize.y, u_windOriginSize.w);

	return texture(u_weatherAerosolsTexture, uv);
}

#ifdef WIND_COMPUTE
/**
 * Calculates the combined wind vector at a given world position.
 * Incorporates macro LBM-derived wind, terrain deflection, and small-scale curl noise.
 * This version takes pre-fetched terrain height and normal for optimization.
 */
vec4 computeWindAtPositionOptimized(vec3 worldPos, float terrainHeight, vec3 normal) {
	if (u_windOriginSize.y <= 0) return vec4(0.0);

	float gridSpacing = u_windParams.x;
	// Measurements are at cell centers, so offset by half spacing for interpolation
	vec2 gridCoord = (worldPos.xz / gridSpacing) - vec2(u_windOriginSize.xz);

	// Normalize to [0, 1] for texture sampling
	vec2 uv = gridCoord / vec2(u_windOriginSize.y, u_windOriginSize.w);

	// float dirPeriod = 1.0;
	// float ampPeriod = gridSpacing;
	// float ampRange = 10.0;

	// float noise1 = 3.14*psrdnoise(dirPeriod*uv+0.01*u_windParams.y, vec2(dirPeriod));
	// vec3 windDir = vec3(cos(noise1), 0.0, sin(noise1));
	// vec3 windDir2 = vec3(windDir.y, -windDir.x);
	// // float noise2 = smoothstep(-1.0, 1.0, psrdnoise(uv*500+0.5*u_windParams.y, vec2(500.0)));
	// // float noise2 = smoothstep(0.35, 1.0, pow(abs(psrdnoise(uv*ampPeriod+windDir.xz*0.15*u_windParams.y, vec2(ampPeriod))), 0.5)  );
	// WorleyData2D dat = worley2d_tiling_id(uv*ampPeriod+windDir.xz*0.15*u_windParams.y, (ampPeriod));
	// float noise2 = step(0.6, hash12Tile(dat.f1, vec2(ampPeriod))) * smoothstep(0.35, 1.0, pow(1.0 - dat.f1_dist, 0.5));
	// //2*(smoothstep(0.35, 1.0, pow(abs(noise(x/2+t)), 0.5)))+(0.5*(smoothstep(0.0, 1.0, pow(abs(noise(x+t)), 1.5))))
	// float noise3 = smoothstep(0.0, 1.0, pow(abs(psrdnoise(uv*2*ampPeriod+windDir.xz*u_windParams.y, vec2(2*ampPeriod))), 1.5)  );
	// vec3 wind = ampRange * smoothstep(0.25, 1.75, 2.0*noise2 + 0.5 * noise3 ) * windDir;
	// return vec4(wind, 0.0);

	// 1. Hardware-accelerated bilinear interpolation of macro wind and drag
	// Use the RAW LBM texture here for integration
	vec4 macroData = texture(u_lbmWindTexture, uv);

	vec3 macroWind = macroData.xyz;
	float drag = macroData.w;
	float macroSpeed = length(macroWind);

	// 2. Terrain Guidance
	// Deflect wind based on terrain normal to follow slopes

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

	return vec4(macroWind, drag);

	float time = u_windParams.y;
	vec2 windDir2D = macroSpeed > 0.001 ? macroWind.xz / macroSpeed : vec2(1.0, 0.0);
	float speedSmoothing = 1.0 / (1.0 + macroSpeed * 0.05);

	// A. Cheap Wavefront Perturbation (Replacing Curl)
	// Use a simple sine-based hash or a very cheap 2D noise to warp the direction
	// This breaks up the wavefront so blades don't move in perfect lockstep
	vec2 warpUV = worldPos.xz * 0.2;
	float warp = sin(warpUV.x + time) * cos(warpUV.y - time);
	vec2 perturbedDir = normalize(windDir2D + vec2(-windDir2D.y, windDir2D.x) * warp * 0.3);

	// B. Phacelle Evaluation
	// Phacelle natively handles the directional alignment. We animate the UVs
	// along the wind direction to create the flow.
	float rippleFreq = 0.15 * speedSmoothing;
	float ripplePhaseSpeed = 5.0 * speedSmoothing;

	// Drift the UVs along the wind direction
	vec2 phacelleUV = (worldPos.xz * rippleFreq) - (windDir2D * time * ripplePhaseSpeed);

	// Assuming a simple Phacelle implementation that returns a normalized vector [x, y]
	// representing cos(phase) and sin(phase)
	vec2 phacelleOut = fastSimplePhacelle2d(phacelleUV, perturbedDir * 5.0);

	// Extract the phase (equivalent to your rawPhasor)
	float rawPhacelle = phacelleOut.x; // Taking the cosine component as the base wave

	// Apply your existing asymmetric power curve
	float positiveRipple = pow(rawPhacelle * 0.5 + 0.5, 2.0);

	// C. Gust Surge Application
	// We can skip the fastSimplex3d entirely and use a lower-frequency Phacelle
	// pass if you need macro gusts, or just rely on the LBM macro data + ripples.
	vec3 finalWind = macroWind;

	if (macroSpeed > 0.001) {
		float surgeStrength = 2.5;
		// Localized surge driven purely by the perturbed Phacelle ripples
		float localizedSurge = positiveRipple * surgeStrength * macroSpeed;
		finalWind.xz += perturbedDir * localizedSurge;
	}

	return vec4(finalWind, drag);
}

/**
 * Calculates the combined wind vector at a given world position.
 * Incorporates macro LBM-derived wind, terrain deflection, and small-scale curl noise.
 */
vec4 computeWindAtPosition(vec3 worldPos) {
	TerrainSurface surface = getTerrainSurface(worldPos.xz);
	return computeWindAtPositionOptimized(worldPos, surface.height, surface.normal);
}
#endif

// Simple hash for random values
float windHash(uint x) {
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = (x >> 16) ^ x;
	return float(x) / 4294967295.0;
}

/**
 * Helper to rotate a vector around an arbitrary axis using Rodrigues' rotation.
 */
vec3 rotateVector(vec3 v, vec3 axis, float angle) {
    if (angle <= 0.0001) return v;
    float cosTheta = cos(angle);
    float sinTheta = sin(angle);
    return v * cosTheta + cross(axis, v) * sinTheta + axis * dot(axis, v) * (1.0 - cosTheta);
}

/**
 * Calculates the combined wind deflection angle and direction.
 */
void getWindDeflectionAngleAndAxis(vec3 basePos, float dist, float v, float windInfluence, float biomeRigidity, float globalWindMultiplier, float globalRigidityMultiplier, uint seed, out float totalBendAngle, out vec3 rotationAxis) {
    float distanceFade = 1.0 - smoothstep(450.0, 750.0, dist);
    vec3 windNoise = (distanceFade > 0.0 && dist < 550.0) ? distanceFade * getWindAtPosition(basePos) : vec3(0.0);
    float windStrength = length(windNoise) * windInfluence * globalWindMultiplier;

    vec3 windDir = (length(windNoise) > 0.001) ? normalize(vec3(windNoise.x, 0.0, windNoise.z)) : vec3(1.0, 0.0, 0.0);

    float maxDeflection = 1.3;
    float rigidity = clamp(biomeRigidity * globalRigidityMultiplier, 0.0, 0.99);
    float windThreshold = rigidity * 2.0;
    float effectiveWindStrength = max(0.0, windStrength - windThreshold);

    // wind_strength scaled to a reasonable angle using tanh
    float resistedWindStrength = maxDeflection * tanh(effectiveWindStrength * 0.15 / maxDeflection);

    float bendFactor = (1.0 - rigidity);
    // Smooth deflection starting slightly above the base
    float windBendAngle = bendFactor * resistedWindStrength * pow(v, 1.2) * smoothstep(0.05, 1.0, v);

    // 2. Base Tilt (static variety)
    float tiltFactor = (windHash(seed + 8888u) * 2.0 - 1.0) * 0.15;
    float tiltAngle = tiltFactor * v;

    // 3. Random gentle sway (low-frequency variety)
    float randomSway = (windHash(seed + 4321u) * 2.0 - 1.0) * 0.05 * v * v;

    vec3 staticDeflection = vec3(randomSway, 0.0, tiltAngle);
    vec3 windDeflection = windDir * windBendAngle;

    vec3 combinedDeflection = staticDeflection + windDeflection;
    totalBendAngle = length(combinedDeflection);

    if (totalBendAngle <= 0.0001) {
        rotationAxis = vec3(0.0, 1.0, 0.0);
    } else {
        vec3 deflectionDir = normalize(combinedDeflection);
        rotationAxis = normalize(cross(vec3(0.0, 1.0, 0.0), deflectionDir));
    }
}

/**
 * Calculates unified wind deflection position using Rodrigues' rotation.
 * Rotates initial relative position in the direction of wind, taking rigidity and distance fade into account.
 * Both grass and ferns can use this to ensure they deflect realistically and resist the wind based on rigidity.
 */
vec3 getWindDeflectedPosition(vec3 initialRelativePos, vec3 basePos, float dist, float v, float windInfluence, float biomeRigidity, float globalWindMultiplier, float globalRigidityMultiplier, uint seed) {
    float totalBendAngle = 0.0;
    vec3 rotationAxis = vec3(0.0, 1.0, 0.0);
    getWindDeflectionAngleAndAxis(basePos, dist, v, windInfluence, biomeRigidity, globalWindMultiplier, globalRigidityMultiplier, seed, totalBendAngle, rotationAxis);
    return rotateVector(initialRelativePos, rotationAxis, totalBendAngle);
}

#endif // HELPERS_WIND_GLSL
