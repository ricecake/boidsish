#ifndef HELPERS_CLOUDS_GLSL
#define HELPERS_CLOUDS_GLSL

#include "../lighting.glsl"
#include "fast_noise.glsl"
#include "math.glsl"
#include "lygia/sdf.glsl"
#include "cloud_utils.glsl"

uniform sampler3D u_cloud3DTexture;


struct CloudSpotDetails {
	float density;
	vec3 relativeExtinction;
	vec3 advectionSpeed;
};

struct CloudDensityResult {
	vec3 density;
	vec3 advectionSpeed;
	float ao;
	vec3 albedo;
	vec3 emissivity;
};

CloudSpotDetails calculateCloudDensity(
	vec3            p,
	CloudWeather    weather,
	CloudLayer      layer,
	CloudProperties props,
	float           time,
	float            simplified,
	float volNoise
) {
	float localFloor, actualThickness;
	float h = getCloudRelativeHeight(p, weather, layer, localFloor, actualThickness);

	vec3 advectSpeed = getCloudAdvectionSpeed(h, time);
	vec3 advect = time * advectSpeed;
	vec3 p_advected = p + advect;

	float type = weather.heightMap;
	float heightGradient = getDensityHeightGradient(h, type);

	float baseNoise = weather.coverage;//getCloud3DCoverage(p_advected, weather, layer, props.worldScale);
	float erodeMask = 1.0 - baseNoise;

	if (simplified <= 1.0) {
		// baseNoise = remapClamp(baseNoise, volNoise * erodeMask, 1.0, 0.0, 1.0);
		baseNoise = remapClamp(volNoise, 1.0 - baseNoise, 1.0, 0.0, 1.0);// * heightGradient;

		if (baseNoise > 0.0 && simplified < 0.25) {
			float detailNoise = abs(fastFbm3d(p_advected / 3000.0));
			baseNoise = remapClamp(baseNoise, detailNoise * erodeMask * 0.5, 1.0, 0.0, 1.0);
		}
	}

	return CloudSpotDetails(
		clamp(baseNoise, 0.00, 1.0),
		vec3(1.0),
		advectSpeed
	);
}

// Cloud density calculation helper
// Returns CloudDensityResult based on world-space position
CloudDensityResult calculateCloudDensityMinimal(
	vec3            p,
	CloudWeather    weather,
	CloudLayer      layer,
	CloudProperties props
) {
	float localFloor, actualThickness;
	float h = getCloudRelativeHeight(p, weather, layer, localFloor, actualThickness);
	vec3 advectSpeed = getCloudAdvectionSpeed(h, time);
	CloudDensityResult pointDetails = CloudDensityResult(vec3(0.0), advectSpeed, 1.0, vec3(1.0), vec3(0.0));
	if (p.y < localFloor || p.y > (localFloor + actualThickness)) {
		return pointDetails;
	}

	CloudSpotDetails res = calculateCloudDensity(p, weather, layer, props, time, 1000.0, 1.0);

	return CloudDensityResult(res.density * res.relativeExtinction, advectSpeed, 1.0, vec3(1.0), vec3(0.0));
}


CloudDensityResult calculateCloudDensity(
	vec3            p,
	CloudWeather    weather,
	CloudLayer      layer,
	CloudProperties props,
	float           time,
	float           simplified
) {
	float localFloor, actualThickness;
	float h = getCloudRelativeHeight(p, weather, layer, localFloor, actualThickness);
	vec3 advectSpeed = getCloudAdvectionSpeed(h, time);
	CloudDensityResult pointDetails = CloudDensityResult(vec3(0.0), advectSpeed, 1.0, vec3(1.0), vec3(0.0));
	if (p.y < localFloor || p.y > (localFloor + actualThickness)) {
		return pointDetails;
	}

	// Sample the 3D cloud volume texture with slower advection speed
	vec3 advect_3d = time * advectSpeed * 0.75;
	vec3 p_advected_3d = p + advect_3d;
	vec3 uvw = vec3(
		p_advected_3d.x / (100000.0 * props.worldScale),
		1.0-h,
		p_advected_3d.z / (100000.0 * props.worldScale)
	);

	// float volumeScale = 20000.0 * props.worldScale;
	// vec3 uvw = p_advected_3d / volumeScale;
	vec4 volSample = textureLod(u_cloud3DTexture, uvw, clamp(simplified * 4.0, 0.0, 4.0));


	// vec4 volSample = textureLod(u_cloud3DTexture, uvw, clamp(simplified * 4.0, 0.0, 4.0));
	float volNoise = volSample.r;
	float volAo = volSample.g;
	float volAlbedoBasis = volSample.b;
	float volDensityBasis = volSample.a;

	CloudSpotDetails res = calculateCloudDensity(p, weather, layer, props, time, simplified, volNoise);

	// Where the current system has density, the 3d volume adds variety and breaks up the linear nature.
	float finalDensity = res.density;
	if (finalDensity > 0.0) {
		finalDensity *= mix(0.4, 1.6, volNoise);
		// finalDensity = adjust(finalDensity, 1.0-volNoise);
	}

	vec3 mixedDensity = res.relativeExtinction * finalDensity;
	// vec3 mixedDensity = vec3(1)*finalDensity;
	vec3 mixedAlbedo = vec3(1.0);//vec3(volAlbedoBasis);

	// return CloudDensityResult(mixedDensity, advectSpeed, volAo, mixedAlbedo, smoothstep(0.8, 1.2, mixedDensity) * vec3(0,1,0));
	return CloudDensityResult(mixedDensity, advectSpeed, volAo, mixedAlbedo, vec3(0.0));
}

#endif // HELPERS_CLOUDS_GLSL
