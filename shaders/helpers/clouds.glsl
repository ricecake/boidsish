#ifndef HELPERS_CLOUDS_GLSL
#define HELPERS_CLOUDS_GLSL

#include "../lighting.glsl"
#include "fast_noise.glsl"
#include "math.glsl"
#include "lygia/generative/random.glsl"
#include "lygia/sdf.glsl"
#include "cloud_utils.glsl"

// Cloud density calculation helper
// Returns vec3(density) based on world-space position, and outputs advection vector
vec3 calculateCloudDensity(
	vec3            p,
	CloudWeather    weather,
	CloudLayer      layer,
	CloudProperties props,
	float           time,
	float           simplified,
	out vec3        advection
) {
	if (p.y < layer.baseFloor || p.y > layer.baseCeiling) {
		float h = (p.y - layer.baseFloor) / layer.thickness;
		advection = getCloudAdvectionSpeed(h, time);
		return vec3(0.0);
	}

	float h = clamp((p.y - layer.baseFloor) / layer.thickness, 0.0, 1.0);
	advection = getCloudAdvectionSpeed(h, time);
	float type = weather.heightMap;
	float heightGradient = getDensityHeightGradient(h, type);
	// Retrieve distance field from evalSdf
	float dist = evalSdf(p, time);

	// Inside the cloud, dist is negative.
	// Density is positive inside, and 0 outside.
	// We can use a smooth transition to avoid aliasing.
	float softness = 15.0 * props.worldScale;
	float density = clamp(-dist * heightGradient, 0.0, 1.0) * props.densityBase;

	return vec3(density);
}

#endif // HELPERS_CLOUDS_GLSL
