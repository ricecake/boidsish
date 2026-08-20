#ifndef HELPERS_CLOUDS_GLSL
#define HELPERS_CLOUDS_GLSL

#include "../lighting.glsl"
#include "fast_noise.glsl"
#include "math.glsl"
#include "lygia/sdf.glsl"
#include "cloud_utils.glsl"

uniform sampler3D u_cloud3DTexture;

/*
// fractional value for sample position in the cloud layer
float GetHeightFractionForPoint(vec3 inPosition, vec2 inCloudMinMax) {
	// get global fractional position in cloud zone
	float height_fraction = (inPosition.z - inCloudMinMax.x )   / (inCloudMinMax.y - inCloudMinMax.x);
	return saturate(height_fraction);
}

// Utility function that maps a value from one range to another.
float Remap(float original_value, float original_min, float original_max, float new_min, float new_max) {
	return new_min + (((original_value - original_min) / (original_max - original_min)) * (new_max - new_min));
}

float SampleCloudDensity(vec3 p, vec3 weather_data, int mip_level,  bool doCheaply) {

	// get height fraction  (be sure to create a cloud_min_max variable)
	float height_fraction = GetHeightFractionForPoint(p, cloud_min_max);

	// wind settings
	vec3 wind_direction = vec3(1.0, 0.0, 0.0);
	float cloud_speed = 10.0;
	// cloud_top offset - push the tops of the clouds along this wind direction by this many units.
	float cloud_top_offset = 500.0;
	// skew in wind direction
	p += height_fraction * wind_direction * cloud_top_offset;
	//animate clouds in wind direction and add a small upward bias to the wind direction
	p+= (wind_direction + vec3(0.0, 0.1, 0.0)  ) * time * cloud_speed;

	// read the low frequency Perlin-Worley and Worley noises
	vec4 low_frequency_noises = tex3Dlod(Cloud3DNoiseTextureA,  Cloud3DNoiseSamplerA, vec4 (p, mip_level) ).rgba;
	// build an fBm out of  the low frequency Worley noises that can be used to add detail to the Low frequency Perlin-Worley noise
	float low_freq_fBm = ( low_frequency_noises.g * 0.625 ) + ( low_frequency_noises.b * 0.25 ) + ( low_frequency_noises.a * 0.125 );
	// define the base cloud shape by dilating it with the low frequency fBm made of Worley noise.
	float base_cloud = Remap( low_frequency_noises.r, - ( 1.0 -  low_freq_fBm), 1.0, 0.0, 1.0 );
	// Get the density-height gradient using the density-height function (not included)
	float density_height_gradient = GetDensityHeightGradientForPoint(height_fraction, weather_data );
	// apply the height function to the base cloud shape
	base_cloud *=  density_height_gradient;

	// cloud coverage is stored in the weather_data’s red channel.
	float cloud_coverage = weather_data.r;

	// apply anvil deformations
	cloud_coverage = pow(cloud_coverage, Remap(height_fraction, 0.7, 0.8, 1.0, lerp(1.0, 0.5, anvil_bias)));

	//Use remapper to apply cloud coverage attribute
	float base_cloud_with_coverage  = Remap(base_cloud, cloud_coverage, 1.0, 0.0, 1.0);
	//Multiply result by cloud coverage so that smaller clouds are lighter and more aesthetically pleasing.
	base_cloud_with_coverage *= cloud_coverage;

	//define final cloud value
	float final_cloud = base_cloud_with_coverage;

	// only do detail work if we are taking expensive samples!
	if(!doCheaply) {

		// add some turbulence to bottoms of clouds using curl noise.  Ramp the effect down over height and scale it by some value (200 in this example)
		vec2 curl_noise = tex2Dlod(Cloud2DNoiseTexture,  Cloud2DNoiseSampler,  vec4 (vec2(p.x, p.y), 0.0, 1.0).rg);
		p.xy += curl_noise.rg * (1.0 - height_fraction) * 200.0;
		// sample high-frequency noises
		vec3 high_frequency_noises = tex3Dlod(Cloud3DNoiseTextureB,  Cloud3DNoiseSamplerB,  vec4 (p * 0.1, mip_level) ).rgb;
		// build High frequency Worley noise fBm
		float high_freq_fBm = ( high_frequency_noises.r * 0.625 ) + ( high_frequency_noises.g * 0.25 ) + ( high_frequency_noises.b * 0.125 );
		// get the height_fraction for use with blending noise types over height
		float height_fraction  = GetHeightFractionForPoint(p, inCloudMinMax);
		// transition from wispy shapes to billowy shapes over height
		float high_freq_noise_modifier = lerp(high_freq_fBm, 1.0 - high_freq_fBm, saturate(height_fraction * 10.0));
		// erode the base cloud shape with the distorted high frequency Worley noises.
		final_cloud = Remap(base_cloud_with_coverage, high_freq_noise_modifier * 0.2 , 1.0, 0.0, 1.0);
	}
	return final_cloud;

}
*/

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

struct VolumeNoise {
	float simplexWorley;
	float worleySmall;
	float worleyMedium;
	float worleyWide;
};

CloudSpotDetails calculateCloudDensity(
	vec3            p,
	CloudWeather    weather,
	CloudLayer      layer,
	CloudProperties props,
	float           time,
	float            simplified,
	vec4 volNoises
) {
	float volNoise = volNoises.r;
	float localFloor, actualThickness;
	float h = getCloudRelativeHeight(p, weather, layer, localFloor, actualThickness);

	vec3 advectSpeed = getCloudAdvectionSpeed(h, time);
	vec3 advect = time * advectSpeed;
	vec3 p_advected = p + advect;

	float type = weather.heightMap;
	float heightGradient = getDensityHeightGradient(h, type);
	// float heightGradient = getDensityHeightGradientForPoint(h, type);

	// float baseNoise = clamp(volNoise * heightGradient, 0, 1);
	// baseNoise *= weather.coverage;

	float baseShape = volNoises.g*0.625 + volNoises.b*0.25 + volNoises.a * 0.125;
	float baseNoise = remapClamp(volNoises.r, baseShape, 1.0, 0.0, 1.0);
	// baseNoise = remapClamp(baseNoise, heightGradient, 1.0, 0.0, 1.0);
	baseNoise *= heightGradient;

	float anvil_bias = 0.7;
	float cloud_coverage = pow(1.0-weather.coverage, remapClamp(h, 0.7, 0.8, 1.0, mix(1.0, 0.5, anvil_bias)));
	baseNoise  = remapClamp(baseNoise, cloud_coverage, 1.0, 0.0, 1.0);


	// baseNoise = remapClamp(baseNoise, 1.0-weather.coverage, 1.0, 0.0, 1.0);

	float erodeMask = 1.0 - baseNoise;
	// baseNoise -= erodeMask * volNoises.g;

	// if (simplified <= 1.0) {
	// 	// baseNoise = remapClamp(baseNoise, volNoise * erodeMask, 1.0, 0.0, 1.0);
	// 	baseNoise = remapClamp(baseNoise, volNoise, 1.0, 0.0, 1.0);// * heightGradient;

	// 	if (baseNoise > 0.0 && simplified < 0.25) {
	// 		float detailNoise = abs(fastFbm3d(p / 3000.0));
	// 		baseNoise = remapClamp(baseNoise, detailNoise * erodeMask * 0.5, 1.0, 0.0, 1.0);
	// 	}
	// 	// if (baseNoise > 0.0 && simplified < 0.25) {
	// 	// 	float fuzzNoise = abs(fastRidge3d(p / 1000.0));
	// 	// 	baseNoise = remapClamp(baseNoise, fuzzNoise * erodeMask * 0.25, 1.0, 0.0, 1.0);
	// 	// }
	// }

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

	CloudSpotDetails res = calculateCloudDensity(p, weather, layer, props, time, 1000.0, vec4(1.0));

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
	vec3 advect_3d = time * max(advectSpeed * 0.75, vec3(50.0, 0.0, 50.0));
	vec3 p_advected_3d = p + advect_3d;
	// vec3 uvw = vec3(
	// 	p_advected_3d.x / (100000.0 * props.worldScale),
	// 	h,
	// 	p_advected_3d.z / (100000.0 * props.worldScale)
	// );

	float volumeScale = 15000.0 * props.worldScale;
	vec3 uvw = p_advected_3d / volumeScale;
	vec4 volSample = textureLod(u_cloud3DTexture, uvw, clamp(simplified * 4.0, 0.0, 4.0));


	// vec4 volSample = textureLod(u_cloud3DTexture, uvw, clamp(simplified * 4.0, 0.0, 4.0));
	// float volNoise = volSample.r;
	// float volAo = volSample.g;
	// float volAlbedoBasis = volSample.b;
	// float volDensityBasis = volSample.a;

	CloudSpotDetails res = calculateCloudDensity(p, weather, layer, props, time, simplified, volSample);

	// Where the current system has density, the 3d volume adds variety and breaks up the linear nature.
	float finalDensity = res.density;
	if (finalDensity > 0.0) {
		// finalDensity *= mix(0.4, 1.6, volNoise);
		// finalDensity = adjust(finalDensity, 1.0-volNoise);
	}

	vec3 mixedDensity = res.relativeExtinction * finalDensity;
	// vec3 mixedDensity = vec3(1)*finalDensity;
	vec3 mixedAlbedo = vec3(1.0);//vec3(volAlbedoBasis);

	// return CloudDensityResult(mixedDensity, advectSpeed, volAo, mixedAlbedo, smoothstep(0.8, 1.2, mixedDensity) * vec3(0,1,0));
	return CloudDensityResult(mixedDensity, advectSpeed, 1.0, mixedAlbedo, vec3(0.0));
}

#endif // HELPERS_CLOUDS_GLSL
